#include "CLogic.h"
#include "logicDef.h"


CLogic::CLogic(CKernel* _m_pKernel){
    // 初始化 Ckernel
    m_pKernel =_m_pKernel;
    // 初始化处理函数映射表
    initpFunc();
}

CLogic::~CLogic(){
    // 删除 CKernel 对象指针
    m_pKernel = 0;
    // 鞋子处理函数映射表
    delete [] m_pFunc;
}


void CLogic::initpFunc(){
    m_pFunc = new func[_DEF_MAP_FUNC_COUNT];
}

void CLogic::autoProtoDeal(int _iFd, char* _szBuf, int _iBufLen){
    int protocal = *(int*)_szBuf;
//    m_pFunc[protocal - _DEF_PROTO_BASE]()
}
