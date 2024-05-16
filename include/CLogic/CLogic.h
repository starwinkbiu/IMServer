#include "logicDef.h"

typedef void (*func)(void* arg);

struct mysql_t{
    mysql_t();
    ~mysql_t();
    char* ip;
    char* username;
    char* password;
    char* database;
    int port;
    void setMysqlInfo(char* ip, char* _username, char* _password, char* _database, int port=3306);
};

class CKernel;
class CMySql;

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
    // 初始化 CMySql
    void initMysql();
    // 设置 sqlInfo
    void setSqlInfo(char* host, char* user, char* pass, char* db, short nport = 3306);
    // kernel 对象, 非本类所有，析构函数无需考虑
    CKernel* m_pKernel;
    // 处理函数映射表
    func* m_pFunc;
    // 数据库信息
    mysql_t m_sqlInfo;
    // 数据库实例
    CMySql* m_pMysql;
};
