#include "CLogic.h"
#include "logicDef.h"
#include "CMySql.h"
#include <string.h>
#include <QDebug>

mysql_t::mysql_t(){
    // 申请各字段空间
    ip = new char[_DEF_MYSQL_IP_MAX_COUNT];
    username = new char[_DEF_MYSQL_USERNAME_MAX_COUNT];
    password = new char[_DEF_MYSQL_PASSWORD_MAX_COUNT];
    database = new char[_DEF_MYSQL_DATABASE_MAX_COUNT];
    // 覆盖为 0
    memset(ip, 0, _DEF_MYSQL_IP_MAX_COUNT);
    memset(username, 0, _DEF_MYSQL_USERNAME_MAX_COUNT);
    memset(password, 0, _DEF_MYSQL_PASSWORD_MAX_COUNT);
    memset(database, 0, _DEF_MYSQL_DATABASE_MAX_COUNT);
    port = 3306;
}

mysql_t::~mysql_t(){
    if(ip){
        delete[] ip;
    }
    if(username){
        delete[] username;
    }
    if(password){
        delete[] password;
    }
    if(database){
        delete[] database;
    }
}

void mysql_t::setMysqlInfo(char* _ip, char* _username, char* _password, char* _database, int _port){
    strncpy(username, _username, _DEF_MYSQL_USERNAME_MAX_COUNT);
    strncpy(password, _password, _DEF_MYSQL_PASSWORD_MAX_COUNT);
    strncpy(database, _database, _DEF_MYSQL_DATABASE_MAX_COUNT);
    port = _port;
}

CLogic::CLogic(CKernel* _m_pKernel){
    // 初始化 Ckernel
    m_pKernel =_m_pKernel;
    // 设置 sqlInfo
    setSqlInfo(_DEF_MYSQL_IP, _DEF_MYSQL_USERNAME, _DEF_MYSQL_PASSWORD, _DEF_MYSQL_DATABASE);
    // 初始化 CMySql
    initMysql();
    // 初始化处理函数映射表
    initpFunc();
}

CLogic::~CLogic(){
    // 删除 CKernel 对象指针
    m_pKernel = 0;
    // 鞋子处理函数映射表
    delete [] m_pFunc;
}

// 初始化协议处理自动机
void CLogic::initpFunc(){
    m_pFunc = new func[_PROTO_MAX_COUNT];
}

void CLogic::autoProtoDeal(int _iFd, char* _szBuf, int _iBufLen){
    int protocal = *(int*)_szBuf;
//    m_pFunc[protocal - _DEF_PROTO_BASE]()
}

// 初始化 CMySql
void CLogic::initMysql(){
    // 初始化实例
    m_pMysql = new CMySql;
    // 连接数据库
    char* ip = m_sqlInfo.ip;
    char* user = m_sqlInfo.username;
    char* pass = m_sqlInfo.password;
    char* db = m_sqlInfo.database;
    if(!m_pMysql->ConnectMySql(ip, user, pass, db)){
        // 连接失败
        cout << __func__ << " error: ";
        cout << "ConnectMySql error" << endl;
    }else{
        cout << "连接数据库成功..." << endl;
    }
}

void CLogic::setSqlInfo(char* host, char* user, char* pass, char* db, short nport){
    strncpy(m_sqlInfo.ip, host, _DEF_MYSQL_IP_MAX_COUNT);
    strncpy(m_sqlInfo.username, user, _DEF_MYSQL_USERNAME_MAX_COUNT);
    strncpy(m_sqlInfo.password, pass, _DEF_MYSQL_USERNAME_MAX_COUNT);
    strncpy(m_sqlInfo.database, db, _DEF_MYSQL_USERNAME_MAX_COUNT);
    m_sqlInfo.port = nport;
}
