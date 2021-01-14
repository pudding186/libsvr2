#pragma once
#include "./db_fields.hpp"

using mysql_operation = enum e_mysql_operation
{
    mysql_select = 0,
    mysql_insert,
    mysql_update,
    mysql_delete,
    mysql_duplicate,
};

using OnError = void(*)(const std::string& err_msg);

extern bool (init_db_manager)(size_t db_thread_num, size_t max_db_event_num, OnError on_error);

extern bool (db_manager_run)(unsigned int run_time = 0);

extern void (uninit_db_manager)(void);

extern bool (db_manager_is_init)(void);

extern HMYSQLCONNECTION (create_mysql_connection)(
    size_t db_thread_idx,
    const char* host,
    unsigned int port,
    const char* user,
    const char* passwd,
    const char* db,
    const char* charact_set);

extern void (destroy_mysql_connection)(HMYSQLCONNECTION connection);

extern HCLIENTMYSQL (db_connection_to_client_mysql)(HMYSQLCONNECTION connection);

extern void db_manager_post_default_cmd(ITable* table, int op_type, SFieldList<>* fields, SFieldList<>* conditions, IResult* result);
extern bool db_manager_do_default_cmd(ITable* table, int op_type, SFieldList<>* fields, SFieldList<>* conditions, IResult* result);

template<typename... Fields, typename... Conditions, typename... Args>
void db_async_select(ITable* table, const SFieldList<Conditions...>& condition, void (*on_record)(bool, const std::vector<SFieldList<Fields...>>&, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;
    using DBResult = RecordResult<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    DBFields* fields = S_NEW(DBFields, 1);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    IResult* result = S_NEW(DBResult, 1, std::bind(on_record, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_select, fields, conditions, result);
}

template<typename... Fields, typename... Conditions, typename C, typename... Args>
void db_async_select(ITable* table, const SFieldList<Conditions...>& condition, C* clss, void (C::*on_record)(bool, const std::vector<SFieldList<Fields...>>&, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;
    using DBResult = RecordResult<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    DBFields* fields = S_NEW(DBFields, 1);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    IResult* result = S_NEW(DBResult, 1, std::bind(on_record, clss, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_select, fields, conditions, result);
}

template<typename... Fields, typename... Conditions>
bool db_sync_select(ITable* table, std::vector<SFieldList<Fields...>>& result, const SFieldList<Conditions...>& condition = SFieldList<>())
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;
    using DBResult = RecordResult<Fields...>;

    if (!db_manager_is_init())
    {
        return false;
    }

    DBFields* fields = S_NEW(DBFields, 1);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    DBResult* record_result = S_NEW(DBResult, 1, nullptr);

    bool ret = db_manager_do_default_cmd(table, mysql_select, fields, conditions, record_result);
    if (ret)
    {
        result = record_result->MoveData();
    }
    else
    {
        result.clear();
    }

    S_DELETE(record_result);
    if (conditions)
    {
        S_DELETE(conditions);
    }
    S_DELETE(fields);

    return ret;
}

template<typename... Conditions>
void db_async_update(ITable* table, const SDynaFieldList& field, const SFieldList<Conditions...>& condition)
{
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SDynaFieldList* fields = db_object_pool<SDynaFieldList>()->New(1, field);
    SDynaFieldList* fields = S_NEW(SDynaFieldList, 1, field);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);//db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    }

    //IResult* result = db_object_pool<AffectResult>()->New(1, nullptr);
    IResult* result = S_NEW(AffectResult, 1, nullptr);

    db_manager_post_default_cmd(table, mysql_update, fields, conditions, result);
}

template<typename... Fields, typename... Conditions>
void db_async_update(ITable* table, const SFieldList<Fields...>& field, const SFieldList<Conditions...>& condition)
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    //SFieldList<Conditions...>* conditions = nullptr;
    DBFields* fields = S_NEW(DBFields, 1, field);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);//db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    }

    IResult* result = S_NEW(AffectResult, 1, nullptr);//db_object_pool<AffectResult>()->New(1, nullptr);

    db_manager_post_default_cmd(table, mysql_update, fields, conditions, result);
}

template<typename... Fields, typename... Conditions, typename... Args>
void db_async_update(ITable* table, const SFieldList<Fields...>& field, const SFieldList<Conditions...>& condition, void (*on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return;
    }

    DBFields* fields = S_NEW(DBFields, 1, field);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);//db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    }

    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_update, fields, conditions, result);
}

template<typename... Fields, typename... Conditions, typename C, typename... Args>
void db_async_update(ITable* table, const SFieldList<Fields...>& field, const SFieldList<Conditions...>& condition, C* clss, void (C::*on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return;
    }

    DBFields* fields = S_NEW(DBFields, 1, field);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);//db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    }

    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, clss, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_update, fields, conditions, result);
}

template<typename... Fields, typename... Conditions>
bool db_sync_update(ITable* table, const SFieldList<Fields...>& field, const SFieldList<Conditions...>& condition, unsigned long long& affect_row)
{
    using DBFields = SFieldList<Fields...>;
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return false;
    }

    DBFields* fields = S_NEW(DBFields, 1, field);
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);//db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    }

    AffectResult* result = S_NEW(AffectResult, 1, nullptr);

    bool ret = db_manager_do_default_cmd(table, mysql_update, fields, conditions, result);
    
    if (ret)
    {
        affect_row = result->GetAffectRow();
    }
    else
    {
        affect_row = 0;
    }

    S_DELETE(result);
    if (conditions)
    {
        S_DELETE(conditions);
    }
    S_DELETE(fields);

    return ret;
}

template<typename... Fields>
void db_async_insert(ITable* table, const SFieldList<Fields...>& field)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    //IResult* result = db_object_pool<AffectResult>()->New(1, nullptr);
    DBFields* fields = S_NEW(DBFields, 1, field);
    IResult* result = S_NEW(AffectResult, 1, nullptr);

    db_manager_post_default_cmd(table, mysql_insert, fields, nullptr, result);
}

template<typename... Fields, typename... Args>
void db_async_insert(ITable* table, const SFieldList<Fields...>& field, void (*on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_insert, fields, nullptr, result);
}

template<typename... Fields, typename C, typename... Args>
void db_async_insert(ITable* table, const SFieldList<Fields...>& field, C* clss, void (C::* on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, clss, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_insert, fields, nullptr, result);
}

template<typename... Fields>
bool db_sync_insert(ITable* table, const SFieldList<Fields...>& field, unsigned long long& affect_row)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return false;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    AffectResult* result = S_NEW(AffectResult, 1, nullptr);

    bool ret = db_manager_do_default_cmd(table, mysql_insert, fields, nullptr, result);

    if (ret)
    {
        affect_row = result->GetAffectRow();
    }
    else
    {
        affect_row = 0;
    }

    S_DELETE(result);
    S_DELETE(fields);

    return ret;
}

template<typename... Fields>
void db_async_duplicate(ITable* table, const SFieldList<Fields...>& field)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    IResult* result = S_NEW(AffectResult, 1, nullptr);

    db_manager_post_default_cmd(table, mysql_duplicate, fields, nullptr, result);
}

template<typename... Fields, typename... Args>
void db_async_duplicate(ITable* table, const SFieldList<Fields...>& field, void (*on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_duplicate, fields, nullptr, result);
}

template<typename... Fields, typename C, typename... Args>
void db_async_duplicate(ITable* table, const SFieldList<Fields...>& field, C* clss, void (C::*on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, clss, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_duplicate, fields, nullptr, result);
}

template<typename... Fields>
bool db_sync_duplicate(ITable* table, const SFieldList<Fields...>& field, unsigned long long& affect_row)
{
    using DBFields = SFieldList<Fields...>;

    if (!db_manager_is_init())
    {
        return false;
    }
    //SFieldList<Fields...>* fields = db_object_pool<SFieldList<Fields...>>()->New(1, field);
    DBFields* fields = S_NEW(DBFields, 1, field);
    AffectResult* result = S_NEW(AffectResult, 1, nullptr);

    bool ret = db_manager_do_default_cmd(table, mysql_duplicate, fields, nullptr, result);

    if (ret)
    {
        affect_row = result->GetAffectRow();
    }
    else
    {
        affect_row = 0;
    }

    S_DELETE(result);
    S_DELETE(fields);

    return ret;
}

template<typename... Conditions>
void db_async_delete(ITable* table, const SFieldList<Conditions...>& condition)
{
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Conditions...>* conditions = nullptr;
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    IResult* result = S_NEW(AffectResult, 1, nullptr);

    db_manager_post_default_cmd(table, mysql_delete, nullptr, conditions, result);
}

template<typename... Conditions, typename... Args>
void db_async_delete(ITable* table, const SFieldList<Conditions...>& condition, void (*on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Conditions...>* conditions = nullptr;

    //if (condition.size())
    //{
    //    conditions = db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    //}
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_delete, nullptr, conditions, result);
}

template<typename... Conditions, typename C, typename... Args>
void db_async_delete(ITable* table, const SFieldList<Conditions...>& condition, C* clss, void (C::* on_affect)(bool, unsigned long long, Args...), Args... args)
{
    using DBConditions = SFieldList<Conditions...>;
    if (!db_manager_is_init())
    {
        return;
    }

    //SFieldList<Conditions...>* conditions = nullptr;

    //if (condition.size())
    //{
    //    conditions = db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    //}
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    IResult* result = S_NEW(AffectResult, 1, std::bind(on_affect, clss, std::placeholders::_1, std::placeholders::_2, args...));

    db_manager_post_default_cmd(table, mysql_delete, nullptr, conditions, result);
}

template<typename... Conditions>
bool db_sync_delete(ITable* table, const SFieldList<Conditions...>& condition, unsigned long long& affect_row)
{
    using DBConditions = SFieldList<Conditions...>;

    if (!db_manager_is_init())
    {
        return false;
    }

    //SFieldList<Conditions...>* conditions = nullptr;

    //if (condition.size())
    //{
    //    conditions = db_object_pool<SFieldList<Conditions...>>()->New(1, condition);
    //}
    DBConditions* conditions = nullptr;

    if (condition.size())
    {
        conditions = S_NEW(DBConditions, 1, condition);
    }

    AffectResult* result = S_NEW(AffectResult, 1, nullptr);

    bool ret = db_manager_do_default_cmd(table, mysql_delete, nullptr, conditions, result);

    if (ret)
    {
        affect_row = result->GetAffectRow();
    }
    else
    {
        affect_row = 0;
    }

    S_DELETE(result);
    if (conditions)
    {
        S_DELETE(conditions);
    }

    return ret;
}

extern std::string (db_check_fields)(ITable* table);

extern std::string (db_check_index)(ITable* table);

extern std::string (db_check_table)(ITable* table);
