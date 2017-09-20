
using namespace CoreIR;

namespace MapperPasses {
class BitOp2Lut : public InstanceVisitorPass {
  public :
    static std::string ID;
    BitOp2Lut() : InstanceVisitorPass(ID,"Does substituions like LT -> GTE + Not") {}
    void setVisitorInfo() override;
};

}

namespace {
#define B0 170
#define B1 (12*17)
#define B2 (15*16)

//Replaces bitnot with lut
bool bitnotReplacement(Instance* inst) {
  Context* c = inst->getContext();
  ModuleDef* def = inst->getContainer();
  string iname = inst->getInstname();
  
  //Add the Lut
  Instance* lut = def->addInstance(iname+"_lut","commonlib.lutN",Args(),{{"init",Const(~B0)}});
  
  //Isolate the instance
  Instance* pt = addPassthrough(inst,"_pt"+c->getUnique());
  def->disconnect(pt->sel("in"),inst);
  def->connect(pt->sel("in")->sel("in"),lut->sel({"in","0"}));
  def->connect(pt->sel("in")->sel("out"),lut->sel("out"));
  
  //Remove instance and inline passthrough
  def->removeInstance(inst);
  inlineInstance(pt);
  return true;
}

//Replaces bitand with lut
bool bitandReplacement(Instance* inst) {
  Context* c = inst->getContext();
  ModuleDef* def = inst->getContainer();
  string iname = inst->getInstname();
  
  //Add the Lut
  Instance* lut = def->addInstance(iname+"_lut","commonlib.lutN",Args(),{{"init",Const(B0 & B1)}});
  
  //Isolate the instance
  Instance* pt = addPassthrough(inst,"_pt"+c->getUnique());
  def->disconnect(pt->sel("in"),inst);
  def->connect(pt->sel("in")->sel("in0"),lut->sel({"in","0"}));
  def->connect(pt->sel("in")->sel("in1"),lut->sel({"in","1"}));
  def->connect(pt->sel("in")->sel("out"),lut->sel("out"));
  
  //Remove instance and inline passthrough
  def->removeInstance(inst);
  inlineInstance(pt);
  return true;
}

//Replaces bitor with lut
bool bitorReplacement(Instance* inst) {
  Context* c = inst->getContext();
  ModuleDef* def = inst->getContainer();
  string iname = inst->getInstname();
  
  //Add the Lut
  Instance* lut = def->addInstance(iname+"_lut","commonlib.lutN",Args(),{{"init",Const(B0 | B1)}});
  
  //Isolate the instance
  Instance* pt = addPassthrough(inst,"_pt"+c->getUnique());
  def->disconnect(pt->sel("in"),inst);
  def->connect(pt->sel("in")->sel("in0"),lut->sel({"in","0"}));
  def->connect(pt->sel("in")->sel("in1"),lut->sel({"in","1"}));
  def->connect(pt->sel("in")->sel("out"),lut->sel("out"));
  
  //Remove instance and inline passthrough
  def->removeInstance(inst);
  inlineInstance(pt);
  return true;
}
//Replaces bitxor with lut
bool bitxorReplacement(Instance* inst) {
  Context* c = inst->getContext();
  ModuleDef* def = inst->getContainer();
  string iname = inst->getInstname();
  
  //Add the Lut
  Instance* lut = def->addInstance(iname+"_lut","commonlib.lutN",Args(),{{"init",Const(B0 ^ B1)}});
  
  //Isolate the instance
  Instance* pt = addPassthrough(inst,"_pt"+c->getUnique());
  def->disconnect(pt->sel("in"),inst);
  def->connect(pt->sel("in")->sel("in0"),lut->sel({"in","0"}));
  def->connect(pt->sel("in")->sel("in1"),lut->sel({"in","1"}));
  def->connect(pt->sel("in")->sel("out"),lut->sel("out"));
  
  //Remove instance and inline passthrough
  def->removeInstance(inst);
  inlineInstance(pt);
  return true;
}

//Replaces bitmux with lut
bool bitmuxReplacement(Instance* inst) {
  Context* c = inst->getContext();
  ModuleDef* def = inst->getContainer();
  string iname = inst->getInstname();
  
  //Add the Lut
  Instance* lut = def->addInstance(iname+"_lut","commonlib.lutN",Args(),{{"init",Const((~B2 & B0) | (B2 & B1))}});
  
  //Isolate the instance
  Instance* pt = addPassthrough(inst,"_pt"+c->getUnique());
  def->disconnect(pt->sel("in"),inst);
  def->connect(pt->sel("in")->sel("in0"),lut->sel({"in","0"}));
  def->connect(pt->sel("in")->sel("in1"),lut->sel({"in","1"}));
  def->connect(pt->sel("in")->sel("sel"),lut->sel({"in","2"}));
  def->connect(pt->sel("in")->sel("out"),lut->sel("out"));
  
  //Remove instance and inline passthrough
  def->removeInstance(inst);
  inlineInstance(pt);
  return true;
}

//Replaces bitconst with lut
bool bitconstReplacement(Instance* inst) {
  Context* c = inst->getContext();
  ModuleDef* def = inst->getContainer();
  string iname = inst->getInstname();
  uint val = inst->getConfigArgs().at("value")->get<int>();
  ASSERT(val==0 || val==1,"invalid val for " + iname + ": " + to_string(val));
  //Add the Lut
  Instance* lut = def->addInstance(iname+"_lut","commonlib.lutN",Args(),{{"init",Const(val)}});
  
  //Isolate the instance
  Instance* pt = addPassthrough(inst,"_pt"+c->getUnique());
  def->disconnect(pt->sel("in"),inst);
  def->connect(pt->sel("in")->sel("out"),lut->sel("out"));
  
  //Remove instance and inline passthrough
  def->removeInstance(inst);
  inlineInstance(pt);
  return true;
}

}


std::string MapperPasses::BitOp2Lut::ID = "bitop2lut";
void MapperPasses::BitOp2Lut::setVisitorInfo() {
  Context* c = this->getContext();
  addVisitorFunction(c->getInstantiable("coreir.bitnot"),bitnotReplacement);
  addVisitorFunction(c->getInstantiable("coreir.bitand"),bitandReplacement);
  addVisitorFunction(c->getInstantiable("coreir.bitor"),bitorReplacement);
  addVisitorFunction(c->getInstantiable("coreir.bitxor"),bitxorReplacement);
  addVisitorFunction(c->getInstantiable("coreir.bitmux"),bitmuxReplacement);
  addVisitorFunction(c->getInstantiable("coreir.bitconst"),bitconstReplacement);
}
