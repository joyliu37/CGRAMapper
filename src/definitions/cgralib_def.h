
using namespace CoreIR;

namespace {

uint flag_sel_size = 4;
enum PE_flag_sel {
  F_EQ=0,
  F_NE=1,
  F_CS=2,
  F_CC=3,
  F_MI=4,
  F_PL=5,
  F_VS=6,
  F_VC=7,
  F_HI=8,
  F_LS=9,
  F_GE=10,
  F_LT=11,
  F_GT=12,
  F_LE=13,
  F_LUT=14,
  F_PRED=15,
};

uint op_size = 6;
enum PE_op {
  OP_ADD=0,
  OP_SUB=1,
  OP_ABS=3,
  OP_GTE_MAX=4,
  OP_LTE_MIN=5,
  OP_EQ=6,
  OP_SEL=8,
  OP_RSHIFT=0xF,
  OP_LSHIFT=0x11,
  OP_MULT_0=0xB,
  OP_MULT_1=0xC,
  OP_MULT_2=0xD,
  OP_RELU=0xE,
  OP_OR=0x12,
  OP_AND=0x13,
  OP_XOR=0x14,
  OP_INV=0x15,
  OP_CNTR=0x18
};
}


//Assumes common has been loaded
void load_mem_ext(Context* c) {
  //Specialized extensions
  Generator* lbmem = c->getGenerator("memory.rowbuffer");
  lbmem->setGeneratorDefFromFun([](Context* c, Values args, ModuleDef* def) {
    uint width = args.at("width")->get<int>();
    uint depth = args.at("depth")->get<int>();
    ASSERT(width==16,"NYI Non 16 bit width");
    ASSERT(depth<=1024,"NYI using mutliple memories");
    Values rbGenargs({{"width",Const::make(c,width)},{"total_depth",Const::make(c,1024)}});
    def->addInstance("cgramem","cgralib.Mem",
      rbGenargs,
      {{"mode",Const::make(c,"linebuffer")},{"depth",Const::make(c,depth)}});
    def->addInstance("c1","corebit.const",{{"value",Const::make(c,true)}});
    def->connect("self.wdata","cgramem.wdata");
    def->connect("self.wen","cgramem.wen");
    def->connect("self.rdata","cgramem.rdata");
    def->connect("self.valid","cgramem.valid");
    def->connect("c1.out","cgramem.cg_en");
    def->connect("c1.out","cgramem.ren");

  });

}

void load_commonlib_ext(Context* c) {
  Generator* smax = c->getGenerator("commonlib.smax");
  smax->setGeneratorDefFromFun([](Context* c, Values args, ModuleDef* def) {
    uint width = args.at("width")->get<int>();
    ASSERT(width==16,"NYI non 16");
    Values PEArgs({
      {"alu_op",Const::make(c,op_size,OP_GTE_MAX)},
      {"flag_sel",Const::make(c,flag_sel_size,F_PRED)},
      {"signed",Const::make(c,true)}
    });
    def->addInstance("cgramax","cgralib.PE",{{"op_kind",Const::make(c,"combined")}},PEArgs);
    def->connect("self.in0","cgramax.data.in.0");
    def->connect("self.in1","cgramax.data.in.1");
    def->connect("self.out","cgramax.data.out");

  });
}

void load_opsubstitution(Context* c) {
  //CoreIR ops
  //op substituions (coreir prims in terms of other coreir prims)
  
  //Changing all the comparison's to <u|s><le|ge>
  vector<string> signs({"u","s"});
  vector<std::pair<string,string>> ops({
    {"gt","le"},
    {"lt","ge"}
  });
  for (auto sign : signs) {
    for (auto op : ops) {
      string from = "coreir." + string(sign)+op.first;
      string to = "coreir." + string(sign)+op.second;
      cout << "from:" <<from << " to:" << to << endl;
      Module* mod = c->getGenerator(from)->getModule({{"width",Const::make(c,16)}});
      ModuleDef* def = mod->newModuleDef();
      def->addInstance("comp",to);
      def->addInstance("not","corebit.not");
      def->connect("self.in0","comp.in0");
      def->connect("self.in1","comp.in1");
      def->connect("comp.out","not.in");
      def->connect("not.out","self.out");
      mod->setDef(def);
    }
  }

  //coreir.neg should be  0 - in
  c->getGenerator("coreir.neg")->setGeneratorDefFromFun([](Context* c, Values args, ModuleDef* def) {
    def->addInstance("sub","coreir.sub");
    def->addInstance("c0","coreir.const",Values(),{{"value",Const::make(c,16,0)}});
    def->connect("self.in","sub.in1");
    def->connect("c0.out","sub.in0");
    def->connect("sub.out","self.out");
  });

}

void load_corebit2lut(Context* c) {
#define B0 170
#define B1 (12*17)
#define B2 (15*16)
  
  {
    //unary
    Module* mod = c->getModule("corebit.not");
    ModuleDef* def = mod->newModuleDef();
    //Add the Lut
    def->addInstance("lut","commonlib.lutN",Values(),{{"init",Const::make(c,8,~B0)}});
    def->addInstance("c0","corebit.const",{{"value",Const::make(c,false)}});
    def->connect("self.in","lut.in.0");
    def->connect("c0.out","lut.in.1");
    def->connect("c0.out","lut.in.2");
    def->connect("lut.out","self.out");
    mod->setDef(def);
  }
  vector<std::pair<string,uint>> binops({{"and",B0&B1},{"or",B0|B1},{"xor",B0^B1}});
  for (auto op : binops) {
    Value* lutval = Const::make(c,8,op.second);
    Module* mod = c->getModule("corebit."+op.first);
    ModuleDef* def = mod->newModuleDef();
    //Add the Lut
    def->addInstance("lut","commonlib.lutN",Values(),{{"init",lutval}});
    def->addInstance("c0","corebit.const",{{"value",Const::make(c,false)}});
    def->connect("self.in0","lut.in.0");
    def->connect("self.in1","lut.in.1");
    def->connect("c0.out","lut.in.2");
    def->connect("lut.out","self.out");
    mod->setDef(def);
  }
  {
    //mux
    Module* mod = c->getModule("corebit.mux");
    ModuleDef* def = mod->newModuleDef();
    //Add the Lut
    def->addInstance("lut","commonlib.lutN",Values(),{{"init",Const::make(c,8,(B2&B1)|((~B2)&B0))}});
    def->connect("self.in0","lut.in.0");
    def->connect("self.in1","lut.in.1");
    def->connect("self.sel","lut.in.2");
    def->connect("lut.out","self.out");
    mod->setDef(def);
  }

}
void laod_cgramapping(Context* c) {
  //commonlib.lut def
  {
    Module* mod = c->getGenerator("commonlib.lutN")->getModule({{"N",Const::make(c,3)}});
    ModuleDef* def = mod->newModuleDef();
    Values bitPEArgs({{"lut_value",mod->getArg("init")}});
    def->addInstance(+"lut","cgralib.PE",{{"op_kind",Const::make(c,"bit")}},bitPEArgs);
    
    def->connect("self.in","lut.bit.in");
    def->connect("lut.bit.out","self.out");
    mod->setDef(def);
  }
  {
    //unary op width)->width
    std::vector<std::tuple<string,uint,bool>> unops({
      {"not",OP_INV,false},
    });
    for (auto op : unops) {
      string opstr = std::get<0>(op);
      uint alu_op = std::get<1>(op);
      bool is_signed = std::get<2>(op);
      cout << "adding to " << opstr << endl;
      Module* mod = c->getGenerator("coreir."+opstr)->getModule({{"width",Const::make(c,16)}});
      ModuleDef* def = mod->newModuleDef();
      Values dataPEArgs({{"alu_op",Const::make(c,op_size,alu_op)},{"signed",Const::make(c,is_signed)}});
      def->addInstance("binop","cgralib.PE",{{"op_kind",Const::make(c,"alu")}},dataPEArgs);
    
      def->connect("self.in","binop.data.in.0");
      def->connect("self.out","binop.data.out");
      mod->setDef(def);
    }
  }
  {
    //binary op (width,width)->width
    std::vector<std::tuple<string,uint,bool>> binops({
      {"add",OP_ADD,false},
      {"sub",OP_SUB,false},
      {"mul",OP_MULT_0,false},
      {"or",OP_OR,false},
      {"and",OP_AND,false},
      {"xor",OP_XOR,false},
      {"ashr",OP_RSHIFT,false},
      {"shl",OP_LSHIFT,false},
    });
    for (auto op : binops) {
      string opstr = std::get<0>(op);
      uint alu_op = std::get<1>(op);
      bool is_signed = std::get<2>(op);
      cout << "adding to " << opstr << endl;
      Module* mod = c->getGenerator("coreir."+opstr)->getModule({{"width",Const::make(c,16)}});
      ModuleDef* def = mod->newModuleDef();
      Values dataPEArgs({{"alu_op",Const::make(c,op_size,alu_op)},{"signed",Const::make(c,is_signed)}});
      def->addInstance("binop","cgralib.PE",{{"op_kind",Const::make(c,"alu")}},dataPEArgs);
    
      def->connect("self.in0","binop.data.in.0");
      def->connect("self.in1","binop.data.in.1");
      def->connect("self.out","binop.data.out");
      mod->setDef(def);
    }
  }
  //Mux
  {
    Module* mod = c->getGenerator("coreir.mux")->getModule({{"width",Const::make(c,16)}});
    ModuleDef* def = mod->newModuleDef();
    Values PEArgs({
      {"alu_op",Const::make(c,op_size,OP_SEL)},
      {"flag_sel",Const::make(c,flag_sel_size,F_PRED)},
      {"signed",Const::make(c,false)}
    });
    def->addInstance("mux","cgralib.PE",{{"op_kind",Const::make(c,"combined")}},PEArgs);
    def->connect("self.in0","mux.data.in.0");
    def->connect("self.in1","mux.data.in.1");
    def->connect("self.sel","mux.bit.in.0");
    def->connect("mux.data.out","self.out");
    mod->setDef(def);
  }
  {
    //comp op (width,width)->bit
    std::vector<std::tuple<string,uint,uint,bool>> compops({
      {"eq",OP_SUB,F_EQ,false},
      {"neq",OP_SUB,F_NE,false},
      {"sge",OP_GTE_MAX,F_PRED,true},
      {"uge",OP_GTE_MAX,F_PRED,false},
      {"sle",OP_LTE_MIN,F_PRED,true},
      {"ule",OP_LTE_MIN,F_PRED,false},
    });
    for (auto op : compops) {
      string opstr = std::get<0>(op);
      uint alu_op = std::get<1>(op);
      uint flag_sel = std::get<2>(op);
      bool is_signed = std::get<3>(op);
      cout << "adding def to " << opstr << endl;
      Module* mod = c->getGenerator("coreir."+opstr)->getModule({{"width",Const::make(c,16)}});
      ModuleDef* def = mod->newModuleDef();
      Values PEArgs({
        {"alu_op",Const::make(c,op_size,alu_op)},
        {"flag_sel",Const::make(c,flag_sel_size,flag_sel)},
        {"signed",Const::make(c,is_signed)}
      });
      def->addInstance("compop","cgralib.PE",{{"op_kind",Const::make(c,"combined")}},PEArgs);
    
      def->connect("self.in0","compop.data.in.0");
      def->connect("self.in1","compop.data.in.1");
      def->connect("self.out","compop.bit.out");
      mod->setDef(def);
    }
  }
  
  //term
  {
    Module* mod = c->getGenerator("coreir.term")->getModule({{"width",Const::make(c,16)}});
    ModuleDef* def = mod->newModuleDef();
    mod->setDef(def); //TODO is this even valid?
  }

  //bitterm
  {
    Module* mod = c->getModule("corebit.term");
    ModuleDef* def = mod->newModuleDef();
    mod->setDef(def); //TODO is this even valid?
  }


}

void LoadDefinition_cgralib(Context* c) {

  load_mem_ext(c);
  load_commonlib_ext(c);
  load_opsubstitution(c);
  load_corebit2lut(c);
  laod_cgramapping(c);

}

