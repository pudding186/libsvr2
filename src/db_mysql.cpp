#include "./lib_svr_common_def.h"
#include "../include/db_mysql.hpp"
#include "../include/loop_cache.h"
#include "../include/timer.h"
#include <queue>
#include <thread>
#include <map>
#include <set>

#ifdef _MSC_VER
#include <windows.h>
#undef max
#undef min
#elif __GNUC__
#include <limits.h>
#include <endian.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <signal.h>
#else
#error "unknown compiler"
#endif

class db_cmd
{
public:
    virtual ~db_cmd() {};
    virtual void OnExecute() = 0;
    virtual void OnResult() = 0;
    virtual void OnRelease() = 0;
protected:
private:
};

using db_queue = struct st_db_queue
{
    HLOOPCACHE  cmd_queue;
    HLOOPCACHE  ret_queue;
    std::queue<db_cmd*>* ret_cache_queue;
};

//using db_obj_pool = struct st_db_obj_pool
//{
//    struct st_db_obj_pool* next_pool;
//    SMemory::IClassMemory* obj_pool;
//};
//
//using db_mem_pool = struct st_db_mem_pool
//{
//    struct st_db_mem_pool* next_pool;
//    HMEMORYMANAGER          mem_pool;
//};

using db_proc = struct st_db_proc
{
    struct st_db_proc*  next_proc;
    db_queue*           queue;
    //db_obj_pool*        db_obj_pool_head;
    //db_mem_pool*        db_mem_pool_head;
    //HCLIENTMYSQL        client_mysql;
    unsigned int        t_id;
    unsigned int        cur_proc_idx;
    bool                is_run;
};

using mysql_connection = struct st_mysql_connection
{
    unsigned long long db_req_mt;
    unsigned long long db_req;
    unsigned long long db_ack;

    size_t  db_thread_idx;

    HCLIENTMYSQL    client_mysql;
    db_proc* create_proc;
};

class db_thread
{
public:
    db_thread();
    ~db_thread();

    size_t _proc_db_cmd();
    void _proc_db_cmd_end();
    void _db_proc_func();

    void start();

    inline void set_idx(size_t idx) { m_idx = idx; }
    inline std::thread& db_thrad_ref(void) { return m_db_thread; }
    inline bool is_run(void) { return m_is_run; }
protected:
private:
    std::thread     m_db_thread;
    size_t          m_idx;
    bool            m_is_run;
};

using db_manager = struct st_db_manager
{
    db_thread*  db_thread_array;
    size_t      db_thread_num;
    size_t      db_queue_size;
    db_proc*    db_proc_head;
    db_proc**   db_proc_main;
    OnError     db_error;
    OnWarn      db_warn;
    bool        is_run;
};

static db_manager* g_db_manager = nullptr;

class CThreadDBProc
{
public:
    CThreadDBProc(void)
    {
        m_db_proc = (db_proc*)malloc(sizeof(db_proc));
        m_db_proc->queue = (db_queue*)malloc(sizeof(db_queue) * g_db_manager->db_thread_num);
        //m_db_proc->client_mysql = nullptr;

        for (size_t i = 0; i < g_db_manager->db_thread_num; i++)
        {
            m_db_proc->queue[i].cmd_queue = create_loop_cache(0, sizeof(db_cmd*) * g_db_manager->db_queue_size);
            m_db_proc->queue[i].ret_queue = create_loop_cache(0, sizeof(db_cmd*) * g_db_manager->db_queue_size);
            m_db_proc->queue[i].ret_cache_queue = new std::queue<db_cmd*>;
        }

        m_db_proc->is_run = true;
        m_db_proc->cur_proc_idx = 0;

#ifdef _MSC_VER
        m_db_proc->t_id = ::GetCurrentThreadId();
        do
        {
            m_db_proc->next_proc = g_db_manager->db_proc_head;
        } while (InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID*>(&(g_db_manager->db_proc_head)),
            m_db_proc,
            m_db_proc->next_proc) != m_db_proc->next_proc);
#elif __GNUC__
        m_db_proc->t_id = syscall(__NR_gettid);
        m_db_proc->next_proc = g_db_manager->db_proc_head;
        while (!__atomic_compare_exchange(
            &g_db_manager->db_proc_head,
            &m_db_proc->next_proc,
            &m_db_proc, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        {
        }
#else
#error "unknown compiler"
#endif
    }

    ~CThreadDBProc(void)
    {
        if (g_db_manager)
        {
            if (m_db_proc)
            {
                m_db_proc->is_run = false;
            }
        }
    }

    inline db_proc* GetDBProc(void) { return m_db_proc; }
    inline db_proc** GetDBProcAddr(void) { return &m_db_proc; }
protected:
    db_proc* m_db_proc;
private:
};

class db_cmd_custom
    :public db_cmd
{
public:
    db_cmd_custom(ITable* table, SDBTask* task)
        :m_table_name(table->GetTableName()), 
        m_mysql_connection(table->Connection()), 
        m_task(task),
        m_warn(""),
        m_error(""){}

    void OnExecute() override
    {
        if (m_mysql_connection->create_proc)
        {
            m_task->OnExecute(m_mysql_connection->client_mysql);
        }
        else
        {
            m_error = fmt::format(u8"Query Fail! ERROR=table {} can't async operator", m_table_name);
        }

        m_mysql_connection->db_ack++;
    }



    void OnResult() override
    {
        if (g_db_manager->db_warn)
        {
            if (m_warn.length())
            {
                g_db_manager->db_warn(m_warn);
            }
        }

        if (g_db_manager->db_error)
        {
            if (m_error.length())
            {
                g_db_manager->db_error(m_error);
            }
        }

        m_task->OnResult();
    }

    void OnRelease() override
    {
        S_DELETE(m_task);
        S_DELETE(this);
    }

protected:
private:
    const std::string&  m_table_name;
    HMYSQLCONNECTION    m_mysql_connection;
    SDBTask*            m_task;
    std::string         m_warn;
    std::string         m_error;
};

class db_cmd_default
    :public db_cmd
{
public:

    db_cmd_default(ITable* table, int op_type, SFieldList<>* fields, SFieldList<>* conditions, IResult* result)
        :m_table_name(table->GetTableName()), m_mysql_connection(table->Connection()), m_fields(fields), m_conditions(conditions), m_result(result), m_op_type(op_type) {}

    db_cmd_default(const std::string& table_name, int op_type, SFieldList<>* fields, SFieldList<>* conditions, IResult* result)
        :m_table_name(table_name), m_mysql_connection(nullptr), m_fields(fields), m_conditions(conditions), m_result(result), m_op_type(op_type) {}

    void Execute(HCLIENTMYSQL client_mysql)
    {
        std::string sql;

        switch (m_op_type)
        {
        case mysql_select:
        {
            sql =
                "SELECT " +
                m_fields->ListSQL() +
                " FROM " +
                "`" +
                m_table_name +
                "`";
        }
        break;
        case mysql_insert:
        {
            sql =
                "INSERT INTO `" +
                m_table_name +
                "`" +
                " (" +
                m_fields->ListSQL() +
                ") VALUES (" +
                m_fields->ValueSQL(client_mysql) + ")";
        }
        break;
        case mysql_update:
        {
            sql =
                "UPDATE `" +
                m_table_name +
                "`" +
                " SET " +
                m_fields->SetSQL(client_mysql);
        }
        break;
        case mysql_delete:
        {
            sql =
                "DELETE FROM `" +
                m_table_name +
                "`";
        }
        break;
        case mysql_duplicate:
        {
            sql =
                "INSERT INTO `" +
                m_table_name +
                "`" +
                " (" +
                m_fields->ListSQL() +
                ") VALUES (" +
                m_fields->ValueSQL(client_mysql) + ")" +
                " ON DUPLICATE KEY UPDATE " +
                m_fields->SetSQL(client_mysql);
        }
        break;
        default:
        {
            return;
        }
        }

        if (m_conditions)
        {
            if (m_conditions->CustomSQL().length())
            {
                sql += " WHERE " + m_conditions->CustomSQL();
            }
            else
            {
                if (m_conditions->size())
                {
                    sql += " WHERE " + m_conditions->AndSQL(client_mysql);
                }
            }
        }

        sql += ";";

        unsigned int query_tick = get_tick();
        CLIENTMYSQLRES res = client_mysql_query(client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));
        query_tick = get_tick() - query_tick;

        if (query_tick > 100)
        {
            m_result->SetWarn(fmt::format(u8"Query Timeout use {}ms! SQL={}", query_tick, sql));
        }

        if (client_mysql_result_success(&res))
        {
            m_result->OnResult(res);
        }
        else
        {
            m_result->SetError(fmt::format(u8"Query Fail! ERROR={}! SQL={}", client_mysql_err(client_mysql), sql));
        }

        client_mysql_free_result(&res);
    }

    void OnExecute() override
    {
        if (m_mysql_connection->create_proc)
        {
            Execute(m_mysql_connection->client_mysql);
        }
        else
        {
            m_result->SetError(fmt::format(u8"Query Fail! ERROR=table {} can't async operator", m_table_name));
        }

        m_mysql_connection->db_ack++;
    }



    void OnResult() override
    {
        if (g_db_manager->db_warn)
        {
            if (m_result->GetWarn().length())
            {
                g_db_manager->db_warn(m_result->GetWarn());
            }
        }

        if (g_db_manager->db_error)
        {
            if (m_result->GetError().length())
            {
                g_db_manager->db_error(m_result->GetError());
            }
        }
        m_result->OnCall();
    }

    void OnRelease() override
    {
        S_DELETE(m_result);
        S_DELETE(m_fields);
        S_DELETE(m_conditions);
        S_DELETE(this);
    }

    //inline void SetClientMysql(HCLIENTMYSQL client_mysql) { m_client_mysql = client_mysql; }
    inline IResult* GetResult(void) { return m_result; }
protected:
private:
    const std::string&  m_table_name;
    HMYSQLCONNECTION    m_mysql_connection;
    SFieldList<>*       m_fields;
    SFieldList<>*       m_conditions;
    IResult*            m_result;
    int                 m_op_type;
};

//////////////////////////////////////////////////////////////////////////
db_thread::db_thread()
{

}

db_thread::~db_thread()
{
    m_db_thread.join();
}

void db_thread::start()
{
    m_db_thread = std::thread(&db_thread::_db_proc_func, this);
}

size_t db_thread::_proc_db_cmd()
{
    size_t proc_count = 0;

    db_proc* proc = g_db_manager->db_proc_head;

    while (proc)
    {
        db_cmd* cmd;

        if (loop_cache_pop_data(proc->queue[m_idx].cmd_queue, &cmd, sizeof(db_cmd*)))
        {
            cmd->OnExecute();
            ++proc_count;

            if (proc->queue[m_idx].ret_cache_queue->empty())
            {
                if (!loop_cache_push_data(proc->queue[m_idx].ret_queue, &cmd, sizeof(db_cmd*)))
                {
                    proc->queue[m_idx].ret_cache_queue->push(cmd);
                }
            }
            else
            {
                proc->queue[m_idx].ret_cache_queue->push(cmd);
            }
        }

        while (!proc->queue[m_idx].ret_cache_queue->empty())
        {
            cmd = proc->queue[m_idx].ret_cache_queue->front();

            if (loop_cache_push_data(proc->queue[m_idx].ret_queue, &cmd, sizeof(db_cmd*)))
            {
                proc->queue[m_idx].ret_cache_queue->pop();
            }
            else
            {
                break;
            }
        }

        proc = proc->next_proc;
    }

    return proc_count;
}

void db_thread::_proc_db_cmd_end()
{
    db_proc* proc = g_db_manager->db_proc_head;
    bool is_busy = false;

    for (;;)
    {
        if (!proc)
        {
            if (is_busy)
            {
                proc = g_db_manager->db_proc_head;
                is_busy = false;
            }
            else
            {
                return;
            }
        }

        db_cmd* cmd;

        if (loop_cache_pop_data(proc->queue[m_idx].cmd_queue, &cmd, sizeof(db_cmd*)))
        {
            cmd->OnExecute();
            is_busy = true;

            if (proc->queue[m_idx].ret_cache_queue->empty())
            {
                if (!loop_cache_push_data(proc->queue[m_idx].ret_queue, &cmd, sizeof(db_cmd*)))
                {
                    proc->queue[m_idx].ret_cache_queue->push(cmd);
                }
            }
            else
            {
                proc->queue[m_idx].ret_cache_queue->push(cmd);
            }
        }

        while (!proc->queue[m_idx].ret_cache_queue->empty())
        {
            is_busy = true;
            cmd = proc->queue[m_idx].ret_cache_queue->front();

            if (loop_cache_push_data(proc->queue[m_idx].ret_queue, &cmd, sizeof(db_cmd*)))
            {
                proc->queue[m_idx].ret_cache_queue->pop();
            }
            else
            {
                break;
            }
        }

        proc = proc->next_proc;
    }
}

void db_thread::_db_proc_func()
{
#ifdef __GNUC__
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

    sigset_t new_set, old_set;
    sigemptyset(&new_set);
    sigemptyset(&old_set);
    sigaddset(&new_set, SIGHUP);
    sigaddset(&new_set, SIGINT);
    sigaddset(&new_set, SIGQUIT);
    sigaddset(&new_set, SIGTERM);
    sigaddset(&new_set, SIGUSR1);
    sigaddset(&new_set, SIGUSR2);
    sigaddset(&new_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &new_set, &old_set);
#endif

    m_is_run = true;

    size_t cur_do_proc_count = 0;
    size_t run_loop_check = 0;
    size_t last_do_proc_count = 0;

    while (g_db_manager->is_run)
    {
        last_do_proc_count = _proc_db_cmd();
        cur_do_proc_count += last_do_proc_count;
        ++run_loop_check;

        if (run_loop_check >= 100)
        {
            run_loop_check = 0;
            cur_do_proc_count = 0;
        }

        if (!last_do_proc_count)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    _proc_db_cmd_end();

    m_is_run = false;
}

CThreadDBProc& GetThreadDBProc(void)
{
    static thread_local CThreadDBProc thread_db_proc;

    return thread_db_proc;
}

db_proc* _get_db_proc(void)
{
    return GetThreadDBProc().GetDBProc();
}

db_proc** _get_db_proc_addr(void)
{
    return GetThreadDBProc().GetDBProcAddr();
}

//void update_db_object_pool(SMemory::IClassMemory* new_obj_pool)
//{
//    db_obj_pool* new_pool = (db_obj_pool*)malloc(sizeof(db_obj_pool));
//    new_pool->obj_pool = new_obj_pool;
//
//    db_proc* proc = _get_db_proc();
//
//    new_pool->next_pool = proc->db_obj_pool_head;
//    proc->db_obj_pool_head = new_pool;
//}

mysql_connection* create_mysql_connection(
    size_t db_thread_idx,
    const char* host,
    unsigned int port,
    const char* user,
    const char* passwd,
    const char* db,
    const char* charact_set)
{
    char err_info[512];

    HCLIENTMYSQL client_mysql = create_client_mysql(
        host, port, user, passwd, db, charact_set, err_info, sizeof(err_info)
        );
    if (!client_mysql)
    {
        if (g_db_manager->db_error)
        {
            g_db_manager->db_error(fmt::format(
                u8"Connect Mysql fail! info:{} {} {} {} {} {} Error:{}",
                host, port, user, passwd, db, charact_set, err_info));
        }

        return nullptr;
    }

    mysql_connection* connection = new mysql_connection;
    connection->db_req_mt = 0;
    connection->db_req = 0;
    connection->db_ack = 0;
    connection->client_mysql = client_mysql;

    if (db_thread_idx)
    {
        connection->create_proc = _get_db_proc();
    }
    else
    {
        connection->create_proc = nullptr;
    }

    connection->db_thread_idx = db_thread_idx % g_db_manager->db_thread_num;

    return connection;
}

void destroy_mysql_connection(mysql_connection* connection)
{
    if (connection)
    {
        while ((connection->db_req + connection->db_req_mt) != connection->db_ack)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        HCLIENTMYSQL client_mysql = connection->client_mysql;

        delete connection;

        destroy_client_mysql(client_mysql);
    }
}

HCLIENTMYSQL db_connection_to_client_mysql(mysql_connection* connection)
{
    if (connection)
    {
        if (!connection->create_proc)
        {
            return connection->client_mysql;
        }
    }

    return nullptr;
}

bool init_db_manager(size_t db_thread_num, size_t max_db_event_num, OnError on_error, OnWarn on_warn)
{
    if (!g_db_manager)
    {
        g_db_manager = new db_manager;
        g_db_manager->is_run = true;
        g_db_manager->db_proc_head = nullptr;
        g_db_manager->db_thread_num = db_thread_num;
        g_db_manager->db_queue_size = max_db_event_num;
        g_db_manager->db_thread_array = new db_thread[db_thread_num];
        g_db_manager->db_proc_main = _get_db_proc_addr();
        g_db_manager->db_error = on_error;
        g_db_manager->db_warn = on_warn;

        for (size_t i = 0; i < db_thread_num; i++)
        {
            g_db_manager->db_thread_array[i].set_idx(i);
            g_db_manager->db_thread_array[i].start();
        }
    }

    return true;
}

void uninit_db_manager(void)
{
    if (g_db_manager)
    {
        g_db_manager->is_run = false;

        db_manager_run(0);

        delete[] g_db_manager->db_thread_array;

        db_proc* proc = g_db_manager->db_proc_head;

        while (proc)
        {
            db_proc* cur_proc = proc;
            proc = proc->next_proc;

            //if (cur_proc->client_mysql)
            //{
            //    destroy_client_mysql(cur_proc->client_mysql);
            //}

            for (size_t i = 0; i < g_db_manager->db_thread_num; i++)
            {
                destroy_loop_cache(cur_proc->queue[i].cmd_queue);
                destroy_loop_cache(cur_proc->queue[i].ret_queue);
                delete cur_proc->queue[i].ret_cache_queue;
            }

            //db_obj_pool* obj_pool = cur_proc->db_obj_pool_head;
            //while (obj_pool)
            //{
            //    db_obj_pool* cur_pool = obj_pool;
            //    obj_pool = obj_pool->next_pool;

            //    delete cur_pool->obj_pool;
            //    free(cur_pool);
            //}

            //db_mem_pool* mem_pool = cur_proc->db_mem_pool_head;
            //while (mem_pool)
            //{
            //    db_mem_pool* cur_pool = mem_pool;
            //    mem_pool = mem_pool->next_pool;

            //    destroy_memory_manager(cur_pool->mem_pool);
            //    free(cur_pool);
            //}

            free(cur_proc->queue);
            free(cur_proc);
            cur_proc = nullptr;
        }

        if (g_db_manager->db_proc_main)
        {
            *(g_db_manager->db_proc_main) = nullptr;
        }

        delete g_db_manager;
        g_db_manager = nullptr;
    }
}

bool db_manager_is_init(void)
{
    return g_db_manager->is_run;
}

bool db_manager_run(unsigned int run_time)
{
    size_t unbusy_proc_count = 0;

    db_proc* proc = _get_db_proc();

    if (run_time)
    {
        unsigned int tick = get_tick();

        for (;;)
        {
            db_cmd* cmd = nullptr;

            if (loop_cache_pop_data(proc->queue[proc->cur_proc_idx].ret_queue, &cmd, sizeof(db_cmd*)))
            {
                unbusy_proc_count = 0;

                cmd->OnResult();

                cmd->OnRelease();
            }
            else
            {
                unbusy_proc_count++;
            }

            proc->cur_proc_idx++;

            if (proc->cur_proc_idx >= g_db_manager->db_thread_num)
            {
                proc->cur_proc_idx = 0;
            }

            if (unbusy_proc_count >= g_db_manager->db_thread_num)
            {
                return false;
            }

            if (get_tick() - tick >= run_time)
            {
                return true;
            }
        }
    }
    else
    {
        for (;;)
        {
            db_cmd* cmd = nullptr;

            if (loop_cache_pop_data(proc->queue[proc->cur_proc_idx].ret_queue, &cmd, sizeof(db_cmd*)))
            {
                unbusy_proc_count = 0;

                cmd->OnResult();

                cmd->OnRelease();
            }
            else
            {
                unbusy_proc_count++;
            }

            proc->cur_proc_idx++;

            if (proc->cur_proc_idx >= g_db_manager->db_thread_num)
            {
                proc->cur_proc_idx = 0;
            }

            if (unbusy_proc_count >= g_db_manager->db_thread_num)
            {
                return false;
            }
        }
    }
}

void db_manager_post_custom_cmd(ITable* table, SDBTask* task)
{
    db_proc* proc = _get_db_proc();

    mysql_connection* connection = table->Connection();

    db_cmd* cmd = S_NEW(db_cmd_custom, 1, table, task);

    while (!loop_cache_push_data(proc->queue[connection->db_thread_idx].cmd_queue, &cmd, sizeof(db_cmd*)))
    {
        std::this_thread::yield();
    }

    if (connection->create_proc == proc)
    {
        connection->db_req++;
    }
    else
    {
#ifdef _MSC_VER
        InterlockedIncrement64((LONG64*)(&connection->db_req_mt));
#elif __GNUC__
        __atomic_add_fetch(&connection->db_req_mt, 1, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
    }
}

void db_manager_post_default_cmd(ITable* table, int op_type, SFieldList<>* fields, SFieldList<>* conditions, IResult* result)
{
    db_proc* proc = _get_db_proc();

    mysql_connection* connection = table->Connection();

    //db_cmd* cmd = db_object_pool<db_cmd_default>()->New(1, table, op_type, fields, conditions, result);
    db_cmd* cmd = S_NEW(db_cmd_default, 1, table, op_type, fields, conditions, result);

    while (!loop_cache_push_data(proc->queue[connection->db_thread_idx].cmd_queue, &cmd, sizeof(db_cmd*)))
    {
        std::this_thread::yield();
    }

    if (connection->create_proc == proc)
    {
        connection->db_req++;
    }
    else
    {
#ifdef _MSC_VER
        InterlockedIncrement64((LONG64*)(&connection->db_req_mt));
#elif __GNUC__
        __atomic_add_fetch(&connection->db_req_mt, 1, __ATOMIC_SEQ_CST);
#else
#error "unknown compiler"
#endif
    }
}

bool db_manager_do_default_cmd(ITable* table, int op_type, SFieldList<>* fields, SFieldList<>* conditions, IResult* result)
{
    db_cmd_default* cmd = S_NEW(db_cmd_default, 1, table->GetTableName(), op_type, fields, conditions, result);

    //if (proc->client_mysql == nullptr)
    //{
    //    char err_info[512];
    //    proc->client_mysql = duplicate_client_mysql(table->Connection()->client_mysql, err_info, sizeof(err_info));

    //    if (proc->client_mysql)
    //    {
    //        cmd->Execute(proc->client_mysql);
    //    }
    //    else
    //    {
    //        cmd->GetResult()->SetError(std::string(err_info));
    //    }
    //}
    //else
    //{
    //    cmd->Execute(proc->client_mysql);
    //}
    if (table->Connection()->create_proc)
    {
        cmd->GetResult()->SetError(fmt::format(u8"Query Fail! ERROR=table {} can't sync operator", table->GetTableName()));;
    }
    else
    {
        cmd->Execute(table->Connection()->client_mysql);
    }

    //SMemory::Delete(cmd);
    S_DELETE(cmd);

    if (g_db_manager->db_warn)
    {
        if (result->GetWarn().length())
        {
            g_db_manager->db_warn(result->GetWarn());
        }
    }

    if (g_db_manager->db_error)
    {
        if (result->GetError().length())
        {
            g_db_manager->db_error(result->GetError());
        }
    }

    return result->GetError().empty();
}

std::string db_check_fields(ITable* table)
{
    std::string err = "";

    std::string sql = fmt::format(u8"SHOW FULL COLUMNS FROM {};", table->GetTableName());

    HCLIENTMYSQL client_mysql = table->Connection()->client_mysql;

    CLIENTMYSQLRES res = client_mysql_query(
        client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

    if (!client_mysql_result_success(&res))
    {
        err = fmt::format(
            u8"check {} fields fail! query sql={} err={}",
            table->GetTableName(),
            sql, 
            client_mysql_err(client_mysql));
        client_mysql_free_result(&res);
        return err;
    }

    std::map<std::string, std::vector<std::string>> db_field_info;

    CLIENTMYSQLROW row = client_mysql_fetch_row(&res);
    while (row.row_values)
    {
        CLIENTMYSQLVALUE value_name = client_mysql_value(row, 0);
        CLIENTMYSQLVALUE value_type = client_mysql_value(row, 1);
        CLIENTMYSQLVALUE value_collation = client_mysql_value(row, 2);
        CLIENTMYSQLVALUE value_null = client_mysql_value(row, 3);
        CLIENTMYSQLVALUE value_default = client_mysql_value(row, 5);
        CLIENTMYSQLVALUE value_extra = client_mysql_value(row, 6);

        auto& info = db_field_info[std::string(value_name.value, value_name.size)];
        info.clear();
        info.push_back(std::string(value_type.value, value_type.size));

        std::string extra = "";
        if (std::string(value_collation.value, value_collation.size).length())
        {
            extra += fmt::format(u8"COLLATE {} ", std::string(value_collation.value, value_collation.size));
        }

        if (std::string(value_null.value, value_null.size) == "NO")
        {
            extra += u8"NOT NULL ";
        }

        if (std::string(value_default.value, value_default.size).length())
        {
            extra += fmt::format(u8"DEFAULT '{}' ", std::string(value_default.value, value_default.size));
        }

        if (std::string(value_extra.value, value_extra.size).length())
        {
            extra += u8" ";
            extra += fmt::format(u8"{} ", std::string(value_extra.value, value_extra.size));
        }

        if (extra.length() > 0)
        {
            extra = extra.substr(0, extra.length() - 1);
        }

        info.push_back(extra);

        row = client_mysql_fetch_row(&res);
    }
    client_mysql_free_result(&res);

    std::vector<std::string> table_desc;

    table->FieldList()->GetDesc(table_desc);

    for (size_t i = 0; i < table_desc.size(); i = i + 3)
    {
        auto it_db_field_info = db_field_info.find(table_desc[i]);

        if (it_db_field_info == db_field_info.end())
        {
            std::string sql_add = fmt::format(
                u8"ALTER TABLE `{}` ADD `{}` {} {};",
                table->GetTableName(), 
                table_desc[i], 
                table_desc[i + 1],
                table_desc[i + 2]
            );

            CLIENTMYSQLRES res_add = client_mysql_query(
                client_mysql, sql_add.c_str(), static_cast<unsigned long>(sql_add.length()));

            if (!client_mysql_result_success(&res_add))
            {
                err = fmt::format(u8"table[{}] add column fail, sql={} err={}",
                    table->GetTableName(), sql_add, client_mysql_err(client_mysql));
                client_mysql_free_result(&res_add);
                return err;
            }
            client_mysql_free_result(&res_add);
        }
        else
        {
            if (it_db_field_info->second[0] != table_desc[i + 1] ||
                it_db_field_info->second[1] != table_desc[i + 2])
            {
                std::string sql_modify = fmt::format(
                    u8"ALTER TABLE `{}` MODIFY COLUMN `{}` {} {};",
                    table->GetTableName(),
                    table_desc[i],
                    table_desc[i + 1],
                    table_desc[i + 2]
                );

                CLIENTMYSQLRES res_modify = client_mysql_query(
                    client_mysql, sql_modify.c_str(), static_cast<unsigned long>(sql_modify.length()));

                if (!client_mysql_result_success(&res_modify))
                {
                    err = fmt::format(u8"table[{}] alter column fail, sql={} err={}",
                        table->GetTableName(), sql_modify, client_mysql_err(client_mysql));
                    client_mysql_free_result(&res_modify);
                    return err;
                }
                client_mysql_free_result(&res_modify);
            }


        }
    }

    return err;
}

std::string _CheckPrimaryKey(
    HCLIENTMYSQL client_mysql,
    const std::string& table_name,
    std::map<std::string, std::string>& db_primary_key_seq,
    SFieldList<>* primary_key)
{
    std::string err = "";

    std::vector<std::string> field_desc;
    primary_key->GetDesc(field_desc);

    bool need_modify = false;

    if (primary_key->size() != db_primary_key_seq.size())
    {
        need_modify = true;
    }
    else
    {
        if (field_desc.size() == 0)
        {
            return fmt::format(u8"table:{} need primary key", table_name);
        }

        auto it_db_primary_key_seq = db_primary_key_seq.begin();
        size_t i = 0;

        for (; i < field_desc.size(); i = i + 4, it_db_primary_key_seq++)
        {
            if (field_desc[i] != it_db_primary_key_seq->second)
            {
                need_modify = true;
                break;
            }
        }
    }
    
    if (need_modify)
    {
        if (db_primary_key_seq.size())
        {
            std::string sql = fmt::format(u8"ALTER TABLE `{}` DROP PRIMARY KEY;", table_name);

            CLIENTMYSQLRES res = client_mysql_query(
                client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

            if (!client_mysql_result_success(&res))
            {
                std::string err_msg = client_mysql_err(client_mysql);


                err = fmt::format(u8"drop primary key fail! query sql={} err={}",
                    sql, client_mysql_err(client_mysql));
                client_mysql_free_result(&res);
                return err;
            }
            client_mysql_free_result(&res);
        }

        std::string sql = fmt::format(u8"ALTER TABLE `{}` ADD PRIMARY KEY ({});",
            table_name, primary_key->ListSQL());

        CLIENTMYSQLRES res = client_mysql_query(
            client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

        if (!client_mysql_result_success(&res))
        {
            std::string err_msg = client_mysql_err(client_mysql);


            err = fmt::format(u8"add primary key fail! query sql={} err={}",
                sql, client_mysql_err(client_mysql));
            client_mysql_free_result(&res);
            return err;
        }

        client_mysql_free_result(&res);
    }

    return "";
}

std::string _CheckIndexKey(
    HCLIENTMYSQL client_mysql,
    const std::string& table_name,
    bool is_unique,
    std::map<std::string, std::map<std::string, std::string>>& db_index_key_seq,
    std::map<std::string, SFieldList<>*> index_key)
{
    std::string err = "";
    for (auto it_index_key = index_key.begin(); it_index_key != index_key.end(); ++it_index_key)
    {
        auto it_db_index_key = db_index_key_seq.find(it_index_key->first);

        if (it_db_index_key == db_index_key_seq.end())
        {
            std::string sql;

            if (is_unique)
            {
                sql = fmt::format(u8"ALTER TABLE `{}` ADD CONSTRAINT `{}` UNIQUE({})",
                    table_name, it_index_key->first, it_index_key->second->ListSQL());
            }
            else
            {
                sql = fmt::format(u8"ALTER TABLE `{}` ADD INDEX `{}` ({})",
                    table_name, it_index_key->first, it_index_key->second->ListSQL());
            }

            CLIENTMYSQLRES res = client_mysql_query(
                client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

            if (!client_mysql_result_success(&res))
            {
                err = fmt::format(u8"add index fail! query sql={} err={}",
                    sql, client_mysql_err(client_mysql));
                client_mysql_free_result(&res);
                return err;
            }
            client_mysql_free_result(&res);
        }
        else
        {
            std::vector<std::string> index_desc;
            it_index_key->second->GetDesc(index_desc);

            bool need_modify = false;

            if (it_index_key->second->size() != it_db_index_key->second.size())
            {
                need_modify = true;
            }
            else
            {
                if (it_index_key->second->size() == 0)
                {
                    return fmt::format(u8"table:{} index key {} is null", table_name, it_index_key->first);
                }

                auto it_db_index_key_seq = it_db_index_key->second.begin();
                size_t i = 0;

                for (; i < index_desc.size(); i = i + 4, it_db_index_key_seq++)
                {
                    if (index_desc[i] != it_db_index_key_seq->second)
                    {
                        need_modify = true;
                        break;
                    }
                }
            }

            if (need_modify)
            {
                std::string sql = fmt::format(u8"ALTER TABLE `{}` DROP INDEX `{}`;",
                    table_name, it_index_key->first);

                CLIENTMYSQLRES res = client_mysql_query(
                    client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

                if (!client_mysql_result_success(&res))
                {
                    err = fmt::format(u8"drop index fail! query sql={} err={}",
                        sql, client_mysql_err(client_mysql));
                    client_mysql_free_result(&res);
                    return err;
                }
                else
                {
                    client_mysql_free_result(&res);

                    if (is_unique)
                    {
                        sql = fmt::format(u8"ALTER TABLE `{}` ADD CONSTRAINT `{}` UNIQUE({})",
                            table_name, it_index_key->first, it_index_key->second->ListSQL());
                    }
                    else
                    {
                        sql = fmt::format(u8"ALTER TABLE `{}` ADD INDEX `{}` ({})",
                            table_name, it_index_key->first, it_index_key->second->ListSQL());
                    }

                    res = client_mysql_query(
                        client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));
                    if (!client_mysql_result_success(&res))
                    {
                        err = fmt::format(u8"add index fail! query sql={} err={}",
                            sql, client_mysql_err(client_mysql));
                        client_mysql_free_result(&res);
                        return err;
                    }

                    client_mysql_free_result(&res);
                }
            }
        }
    }

    return err;
}

std::string db_check_index(ITable* table)
{
    std::string err = "";

    HCLIENTMYSQL client_mysql = table->Connection()->client_mysql;

    std::string sql = fmt::format(u8"show index from {};", table->GetTableName());

    CLIENTMYSQLRES res = client_mysql_query(
        client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

    if (!client_mysql_result_success(&res))
    {
        err = fmt::format(u8"check index fail! query sql={} err={}",
            sql, client_mysql_err(client_mysql));
        client_mysql_free_result(&res);
        return err;
    }

    std::map<std::string, std::map<std::string, std::string>> db_primary_key_seq;
    std::map<std::string, std::map<std::string, std::string>> db_unique_key_seq;
    std::map<std::string, std::map<std::string, std::string>> db_index_key_seq;

    CLIENTMYSQLROW row = client_mysql_fetch_row(&res);
    while (row.row_values)
    {
        CLIENTMYSQLVALUE value;

        value = client_mysql_value(row, 0);
        std::string table_name(value.value, value.size);

        value = client_mysql_value(row, 1);
        std::string non_unique(value.value, value.size);

        value = client_mysql_value(row, 2);
        std::string key_name(value.value, value.size);

        value = client_mysql_value(row, 3);
        std::string seq_in_index(value.value, value.size);

        value = client_mysql_value(row, 4);
        std::string column_name(value.value, value.size);

        if (key_name == "PRIMARY")
        {
            db_primary_key_seq[key_name][seq_in_index] = column_name;
        }
        else
        {
            if (non_unique == "0")
            {
                db_unique_key_seq[key_name][seq_in_index] = column_name;
            }
            else
            {
                db_index_key_seq[key_name][seq_in_index] = column_name;
            }
        }

        row = client_mysql_fetch_row(&res);
    }
    client_mysql_free_result(&res);

    std::map<std::string, std::string> primary_key;

    if (db_primary_key_seq.size())
    {
        primary_key = db_primary_key_seq.begin()->second;
    }

    err = _CheckPrimaryKey(
        client_mysql,
        table->GetTableName(), 
        primary_key, 
        table->PrimaryKey());

    if (err != "")
    {
        return err;
    }

    err = _CheckIndexKey(
        client_mysql,
        table->GetTableName(),
        true,
        db_unique_key_seq,
        table->UniqueKey());

    if (err != "")
    {
        return err;
    }

    err = _CheckIndexKey(
        client_mysql,
        table->GetTableName(),
        false,
        db_index_key_seq,
        table->IndexKey());

    return err;
}

std::string db_check_table(ITable* table)
{
    std::string sql = u8"CREATE TABLE IF NOT EXISTS ";

    sql += u8"`" + table->GetTableName() + u8"` (";
    sql += table->FieldList()->CreateSQL();
    sql += "PRIMARY KEY (" + table->PrimaryKey()->ListNameSQL() + ")";

    std::map<std::string, SFieldList<>*> unique_keys = table->UniqueKey();

    for (auto it = unique_keys.begin(); it != unique_keys.end(); ++it)
    {
        sql += u8", UNIQUE KEY `" + it->first + u8"` (";
        sql += it->second->ListNameSQL() + u8")";
    }

    std::map<std::string, SFieldList<>*> index_keys = table->IndexKey();

    for (auto it = index_keys.begin(); it != index_keys.end(); ++it)
    {
        sql += u8", KEY `" + it->first + u8"` (";
        sql += it->second->ListNameSQL() + u8")";
    }

    sql += u8")ENGINE=" + table->GetTableEngine();
    sql += u8" DEFAULT CHARSET=" + table->GetTableCharset();
    sql += u8" ROW_FORMAT=" + table->GetTableRowFormat();
    sql += u8" ";
    sql += table->GetTableExtra();
    sql += ";";

    CLIENTMYSQLRES res = client_mysql_query(
        table->Connection()->client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

    if (!client_mysql_result_success(&res))
    {
        std::string err = fmt::format(u8"check table fail! query sql={} err={}",
            sql, client_mysql_err(table->Connection()->client_mysql));
        client_mysql_free_result(&res);
        return err;
    }
    client_mysql_free_result(&res);

    return "";
}

std::vector<SRecordFieldList> Record2FieldList(CLIENTMYSQLRES& res)
{
    std::vector<SRecordFieldList> datas;
    datas.resize(client_mysql_rows_num(&res));

    for (auto& field : datas)
    {
        field.Init(res);
        field.LoadData(client_mysql_fetch_row(&res));
    }

    return datas;
}

bool db_has_field(ITable* table, const std::string& field_name, std::string& err)
{
    std::string sql = fmt::format(u8"SHOW FULL COLUMNS FROM {};", table->GetTableName());

    HCLIENTMYSQL client_mysql = table->Connection()->client_mysql;

    CLIENTMYSQLRES res = client_mysql_query(
        client_mysql, sql.c_str(), static_cast<unsigned long>(sql.length()));

    if (!client_mysql_result_success(&res))
    {
        err = fmt::format(
            u8"check {} fields fail! query sql={} err={}",
            table->GetTableName(),
            sql, 
            client_mysql_err(client_mysql));
        client_mysql_free_result(&res);
        return false;
    }

    bool has_field = false;
    CLIENTMYSQLROW row = client_mysql_fetch_row(&res);
    while (row.row_values)
    {
        CLIENTMYSQLVALUE value_name = client_mysql_value(row, 0);
        if (!strcmp(std::string(value_name.value, value_name.size).c_str(), field_name.c_str()))
        {
            has_field = true;
            break;
        }

        row = client_mysql_fetch_row(&res);
    }
    client_mysql_free_result(&res);

    return has_field;
}
