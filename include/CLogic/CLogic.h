
#define _DEF_MAP_FUNC_COUNT         (20)

typedef void (*func)(void* arg);

class CKernel;

class CLogic{
public:
    CLogic(CKernel* _m_pKernel);
    ~CLogic();
    // 初始化处理函数映射表
    void initpFunc();
    // 心脏机制回复
    void heartAliveRs(void* arg);
    // 协议处理自动机
    void autoProtoDeal(int _iFd, char* _szBuf, int _iBufLen);
    // kernel 对象, 非本类所有，析构函数无需考虑
    CKernel* m_pKernel;
    // 处理函数映射表
    func* m_pFunc;
};
