/**
* Tencent is pleased to support the open source community by making DCache available.
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
* Licensed under the BSD 3-Clause License (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSD-3-Clause
*
* Unless required by applicable law or agreed to in writing, software distributed under
* the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
* either express or implied. See the License for the specific language governing permissions
* and limitations under the License.
*/
#include "servant/Application.h"

#include "Assistance.h"

const long Assistance::C_PAGE_BEGIN = 0;
const long Assistance::C_PAGE_END   = 429496;
const int  Assistance::C_SUCCESS    = 1;

int Assistance::hasOutOfBounds(string page)
{
    int tmp = atol(page.c_str());
    if ((tmp > C_PAGE_END) || (tmp < C_PAGE_BEGIN)) return -1;
    return C_SUCCESS;
}

void Assistance::getIPAndPort(const string& endPoint, string& ip, string& port)
{
    ip = endPoint.substr(endPoint.find_first_of('h',0)+2);
    char *tmp_s = (char*)malloc(ip.length());
    ip.copy(tmp_s,ip.length(),0);
    ip = strtok(tmp_s," ");
    free(tmp_s);
    port = endPoint.substr(endPoint.find("-p",0)+3);
}

int Tool::getNodeObj(TC_Mysql &mysqlTarsDb, const string & sIp, string & sNodeObj)
{
    time_t now = time(NULL);

    try
    {
        string sQuerySql = "select node_obj, UNIX_TIMESTAMP(last_heartbeat) as time from t_node_info where node_name='" + mysqlTarsDb.escapeString(sIp) + "'";
        TC_Mysql::MysqlData nodeData;
        nodeData = mysqlTarsDb.queryRecord(sQuerySql);
        if (nodeData.size() == 1)
        {
            if ((TC_Common::strto<int>(nodeData[0]["time"]) + 60*5) > now)
            {
                sNodeObj = nodeData[0]["node_obj"];
                return 0;
            }
            else
            {
                TLOGERROR(FUN_LOG << "node's last_heartbeat is old:" << nodeData[0]["time"] << ", now is:"<< now << "|node name:" << mysqlTarsDb.escapeString(sIp) << endl);
                return -1;
            }
        }

    }
    catch (exception &ex)
    {
        TLOGERROR(FUN_LOG << "query from t_node_info catch exception:" << ex.what() << endl);
    }

    return -1;
}

void Tool::parseServerName(const string & sFullServerName, string & application, string & servername)
{
    string::size_type pos = sFullServerName.find_first_of(".");
    if (string::npos == pos)
    {
        TLOGERROR(FUN_LOG << "server name is invalild, right eg: Test.TestServer|server name:" << sFullServerName << endl);
        throw runtime_error("server name is invalild, right eg: Test.TestServer|server name:" + sFullServerName);
    }

    application = sFullServerName.substr(0, pos);
    servername  = sFullServerName.substr(pos + 1);
}

bool Tool::isInactive(TC_Mysql &mysqlTarsDb, const string & application, const string & servername, const string & host, string & sError)
{
    //检查服务状态是否完全inactive
    string sQuerySql = "select setting_state, present_state from t_server_conf where application='" + application + "' and server_name='" + servername + "' and node_name='" + host + "'";
    TC_Mysql::MysqlData serverData;
    serverData = mysqlTarsDb.queryRecord(sQuerySql);
    int iRecordCount = serverData.size();
    if (iRecordCount ==1)
    {
        if (serverData[0]["present_state"] != "inactive")
        {
            sError = "server's state is not inactive|app name:" + application + "|server name:" + servername + "|ip:" + host + "|setting_state:" + serverData[0]["setting_state"] + "|present_state:" + serverData[0]["present_state"];
            TLOGERROR(FUN_LOG << sError << endl);
            return false;
        }
    }
    else
    {
        if (iRecordCount == 0)
        {
            sError = "server is not existed|app name:" + application + "|server name:" + servername + "|ip:" + host + "|sql:" + sQuerySql;
        }
        else
        {
            sError = "query server result count:" + TC_Common::tostr(serverData.size()) + " more than 1|app name:" + application + "|server name:" + servername + "|ip:" + host + "|sql:" + sQuerySql;
        }

        TLOGERROR(FUN_LOG << sError << endl);
        return false;
    }

    return true;
}

void Tool::delRouteInfo4CacheServer(TC_Mysql &mysqlRouterDb, const string & CacheServer)
{
    //删除服务器信息
    string sWhere = "where server_name='" + CacheServer + "'";
    mysqlRouterDb.deleteRecord("t_router_server", sWhere);

    //删除服务组中的相关信息
    mysqlRouterDb.deleteRecord("t_router_group", sWhere);
}

void Tool::delConfigInfo4CacheServer(TC_Mysql &mysqlRelationDb, const string & sFullCacheServer)
{
    //删除服务器信息
    string sWhere = "where server_name='" + sFullCacheServer + "' and level=2";
    mysqlRelationDb.deleteRecord("t_config_table", sWhere);
}

void Tool::delReferConfig4CacheServer(TC_Mysql &mysqlRelationDb, const string & sFullCacheServer)
{
    //删除关联配置信息
    string sWhere = "where server_name='" + sFullCacheServer + "'";
    mysqlRelationDb.deleteRecord("t_config_reference", sWhere);
}

void Tool::delRelation4CacheServer(TC_Mysql &mysqlRelationDb, const string & sFullCacheServer)
{
    //删除关联配置信息
    string sWhere = "where cache_name ='" + sFullCacheServer.substr(7) + "'";
    mysqlRelationDb.deleteRecord("t_cache_router", sWhere);
}

int Tool::cleanProxyConf(TC_Mysql &mysqlRelationDb, const string &proxyName)
{
    TLOGDEBUG(FUN_LOG << "proxy server name:" << proxyName << endl);

    try
    {
        string sql = "select * from t_proxy_app where proxy_name = '" + proxyName +"'";
        TC_Mysql::MysqlData proxyData = mysqlRelationDb.queryRecord(sql);

        if (proxyData.size() == 1)
        {
            string sWhere = "where id = '" + proxyData[0]["id"] +"'";
            int n = mysqlRelationDb.deleteRecord("t_proxy_app", sWhere);

            TLOGDEBUG(FUN_LOG << "delete t_proxy_app affect row:" << n << "|proxy server name:" << proxyName << endl);
        }
        else if (proxyData.size() > 1)
        {
            TLOGERROR(FUN_LOG << "query proxy-app count:" << proxyData.size() << " more than 1|proxy server name:" << proxyName << endl);
            return -1;
        }
    }
    catch(exception &ex)
    {
        TLOGERROR(FUN_LOG << "catch exception:" << ex.what() << endl);
        return -1;
    }

    return 0;
}

int Tool::cleanRouterConf(TC_Mysql &mysqlRelationDb, const string &routerName)
{
    TLOGDEBUG(FUN_LOG << "router server name:" << routerName << endl);

    try
    {
        string sql = "select * from t_router_app where router_name = '" + routerName + "'";
        TC_Mysql::MysqlData routerData = mysqlRelationDb.queryRecord(sql);
        if (routerData.size() == 1)
        {
            string sWhere = "where id = '" + routerData[0]["id"] +"'";
            int n = mysqlRelationDb.deleteRecord("t_router_app", sWhere);

            TLOGDEBUG(FUN_LOG << "delete t_router_app affect row:" << n << "|router server name:" << routerName << endl);
        }
        else if (routerData.size() > 1)
        {
            TLOGERROR(FUN_LOG << "query router-app count:" << routerData.size() << " more than 1|router server name:" << routerName << endl);
            return -1;
        }

        sql = "select * from t_proxy_router where router_name='" + routerName + ".RouterObj'";
        TC_Mysql::MysqlData prdata = mysqlRelationDb.queryRecord(sql);
        if (prdata.size() == 1)
        {
            string sWhere = "where id = '" + prdata[0]["id"] +"'";
            int n = mysqlRelationDb.deleteRecord("t_proxy_router", sWhere);

            TLOGDEBUG(FUN_LOG << "delete t_proxy_router affect row:" << n << "|router server name:" << routerName << endl);
        }
        else if (prdata.size() > 1)
        {
            TLOGERROR(FUN_LOG << "query proxy-router count:" << routerData.size() << " more than 1|router server name:" << routerName << endl);
            return -1;
        }
    }
    catch(exception &ex)
    {
        TLOGERROR(FUN_LOG << "catch exception:" << ex.what() << endl);
        return -1;
    }
    return 0;
}

int Tool::cleanDBaccessConf(TC_Mysql &mysqlRelationDb, const string &dbaccessName)
{
    TLOGDEBUG(FUN_LOG << "dbaccess server name:" << dbaccessName << endl);

    try
    {
        string sql = "select * from t_dbaccess_app where dbaccess_name = '" + dbaccessName +"'";
        TC_Mysql::MysqlData dbaccessData = mysqlRelationDb.queryRecord(sql);

        if (dbaccessData.size() == 1)
        {
            string sWhere = "where id = '" + dbaccessData[0]["id"] +"'";
            int n = mysqlRelationDb.deleteRecord("t_dbaccess_app", sWhere);

            TLOGDEBUG(FUN_LOG << "delete t_dbaccess_app affect row:" << n << "|dbaccess server name:" << dbaccessName << endl);
        }
        else if (dbaccessData.size() > 1)
        {
            TLOGERROR(FUN_LOG << "query dbaccess-app count:" << dbaccessData.size() << " more than 1|dbaccess server name:" << dbaccessName << endl);
            return -1;
        }
    }
    catch(exception &ex)
    {
        TLOGERROR(FUN_LOG << "catch exception:" << ex.what() << endl);
        return -1;
    }

    return 0;
}

void Tool::UninstallCacheServer(TC_Mysql &mysqlRouterDb, TC_Mysql &mysqlTarsDb, TC_Mysql &mysqlRelationDb, const string &sFullCacheServer, const string &sCacheBakPath, bool checkNode)
{
    try
    {
        string sQuerySql = "select ip from t_router_server where server_name='" + sFullCacheServer + "'";
        TC_Mysql::MysqlData cacheData = mysqlRouterDb.queryRecord(sQuerySql);
        if (cacheData.size() == 1)
        {
            string sApplication;
            string sServerName;
            string sError;

            parseServerName(sFullCacheServer, sApplication, sServerName);

            if (!isInactive(mysqlTarsDb, sApplication, sServerName, cacheData[0]["ip"], sError))
            {
                throw runtime_error(sError);
            }

            if (checkNode == true)
            {
                //尝试从配置文件中获取共享内存的key
                string sKey("");

                sQuerySql = "select id from t_config_item where item='ShmKey' and path='Main/Cache'";
                TC_Mysql::MysqlData shmKeyId;
                shmKeyId = mysqlRelationDb.queryRecord(sQuerySql);

                sQuerySql = "select * from t_config_table where server_name='" + sFullCacheServer + "' and item_id=" + shmKeyId[0]["id"];
                TC_Mysql::MysqlData shmKeyData;
                shmKeyData = mysqlRelationDb.queryRecord(sQuerySql);
                if (shmKeyData.size() == 1)
                {
                    sKey = shmKeyData[0]["config_value"];
                }

                //备份共享内存
                string sNodeObj("");
                if (-1 == getNodeObj(mysqlTarsDb, cacheData[0]["ip"], sNodeObj))
                {
                    TLOGERROR(FUN_LOG << "failed to get node obj|cache server name:" + sFullCacheServer + "|ip:" + cacheData[0]["ip"] << endl);
                }
                else
                {
                    NodePrx nodePrx = Application::getCommunicator()->stringToProxy<NodePrx>(sNodeObj);
                    try
                    {
                        int iRet = nodePrx->delCache(sFullCacheServer, sCacheBakPath, sKey, sError);
                        if (iRet == -1)
                        {
                            throw runtime_error(sError);
                        }
                    }
                    catch(exception &e)
                    {
                        string sError =  e.what();
                        TLOGERROR(FUN_LOG << "del cache catch exception:" + sError << endl);
                        if (string::npos == sError.find("timeout"))
                        {
                            throw runtime_error(e.what());
                        }
                    }
                }
            }

            sQuerySql = "select * from t_config_table where server_name='" + sFullCacheServer + "' and level = 2";
            TC_Mysql::MysqlData sConfigData;
            sConfigData = mysqlRelationDb.queryRecord(sQuerySql);
            if (sConfigData.size() >= 1)
            {
                for (size_t i=0; i<sConfigData.size(); ++i)
                {
                    delConfigInfo4CacheServer(mysqlRelationDb,sFullCacheServer);
                }
            }

            //删除路由库中的服务相关信息
            delRouteInfo4CacheServer(mysqlRouterDb, sFullCacheServer);

            delRelation4CacheServer(mysqlRelationDb,sFullCacheServer);

            UninstallTarsServer(mysqlTarsDb, sFullCacheServer, cacheData[0]["ip"], sError);

        }
        else
        {
            TLOGERROR(FUN_LOG << "query from t_router_server can not find cache server's ip or result size:" + TC_Common::tostr(cacheData.size()) + " more than one|cache server name:" + sFullCacheServer << endl);
            throw runtime_error("uninstall dcache error: cannot find cache server "+ sFullCacheServer + "'s ip or more than one:" + TC_Common::tostr(cacheData.size()));
        }
    }
    catch (exception &ex)
    {
        TLOGERROR(FUN_LOG << "query from t_router_server catch exception:" << ex.what() << endl);
    }
}

int Tool::UninstallTarsServer(TC_Mysql &mysqlTarsDb, const string &sTarsServerName, const  string &sIp, std::string &sError)
{
    try
    {
        if (sTarsServerName.empty() || sIp.empty())
        {
            sError = "server name or ip is empty|server name:" + sTarsServerName + "|ip:"+ sIp;
            TLOGERROR(FUN_LOG << sError << endl);
            return -1;
        }

        string sApplication;
        string sServerName;
        parseServerName(sTarsServerName, sApplication, sServerName);

        //检查服务状态是否完全inactive
        if (!isInactive(mysqlTarsDb, sApplication, sServerName, sIp, sError))
        {
            return -1;
        }

        //注意:只要删除db_taf信息成功就返回0，以下始终返回0
        //删路径
        try
        {
            string sNodeObj("");
            if (-1 == getNodeObj(mysqlTarsDb, sIp, sNodeObj))
            {
                sError = "failed to get node obj|server name:" + sTarsServerName + "|ip:" + sIp;
                TLOGERROR(FUN_LOG << sError << endl);
            }
            else
            {
                TC_Config tcConf;
                tcConf.parseFile(ServerConfig::BasePath + "DCacheOptServer.conf");

                int unstallTimeOut = TC_Common::strto<int>(tcConf.get("/Main/Uninstall<Timeout>", "10"));

                string sSql = "update t_server_conf set registry_timestamp='" + TC_Common::tm2str(TNOW) + "' where application='" + mysqlTarsDb.escapeString(sApplication) + "' and server_name='" + mysqlTarsDb.escapeString(sServerName) + "'";
                mysqlTarsDb.execute(sSql);

                NodePrx nodePrx = Application::getCommunicator()->stringToProxy<NodePrx>(sNodeObj);
                nodePrx->tars_timeout(unstallTimeOut * 1000);
                int iRet = 0;
                try
                {
                    iRet = nodePrx->destroyServer(sApplication, sServerName, sError);
                }
                catch(exception &ex)
                {
                    //如果异常值不是 -10，就返回错误，否则也可以继续下去
                    if (iRet != -10)
                    {
                        sError = string("destroy server catch exception:") + ex.what();
                        TLOGERROR(FUN_LOG << sError << endl);
                        return -1;
                    }
                    else
                        iRet = 0;
                }

                if ((iRet == 0) || (iRet == -1) || (iRet == -2))
                {
                    if (iRet == -1)
                    {
                        if (sError.find("not exist") == string::npos)
                        {
                            return -1;
                        }
                    }
                }
                else
                {
                    return -1;
                }
            }

            //删除taf服务本身的记录
            string sWhere = "where application='" + sApplication + "' and server_name='" + sServerName + "' and node_name='" + sIp + "'";
            mysqlTarsDb.deleteRecord("t_adapter_conf", sWhere);
            mysqlTarsDb.deleteRecord("t_server_conf", sWhere);

            //删除节点配置
            sWhere = "where server_name ='" + sApplication + "."+sServerName+ "' and host='"+ sIp + "' and level=3";
            mysqlTarsDb.deleteRecord("t_config_files", sWhere);

            string sql = "select * from t_server_conf where application='" + sApplication + "' and server_name='" + sServerName + "'";
            TC_Mysql::MysqlData data = mysqlTarsDb.queryRecord(sql);
            if (data.size() == 0)
            {
                sWhere = "where server_name ='" + sApplication + "."+sServerName+ "' and level=2";
                mysqlTarsDb.deleteRecord("t_config_files", sWhere);
            }

            TLOGDEBUG(FUN_LOG << "success to destroy server remotely|server name:" << sTarsServerName << endl);
            return 0;
        }
        catch(exception &ex)
        {
            sError = string("operate db catch exception:") + ex.what();
            TLOGERROR(FUN_LOG << sError << endl);
        }
        catch(...)
        {
            sError = "operate db catch unknow exception";
            TLOGERROR(FUN_LOG << sError << endl);
        }

        return 0;
    }
    catch(exception &ex)
    {
        sError = string("catch exception:") + ex.what();
        TLOGERROR(FUN_LOG << sError << endl);
    }
    catch(...)
    {
        sError = "catch unknow exception";
        TLOGERROR(FUN_LOG << sError << endl);
    }

    return -1;
}

