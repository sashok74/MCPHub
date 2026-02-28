#pragma once
// CodeTemplates.h — raw string templates for all MERPHelper code generators
// Each template uses {PLACEHOLDER} syntax, rendered via CodeGenerator::Render()
// TPL_QUERY_TYPES uses {{MUSTACHE}} syntax, rendered via TemplateEngine::RenderEx()

//=============================================================================
// 0. TPL_QUERY_TYPES — GenerateTemplateClassCode (Mustache template)
//=============================================================================
static const char* TPL_QUERY_TYPES = R"(template <>
struct InParam<Querys::{{QUERY_NAME}}> {
{{#IN_PARAMS}}    {{CPP_TYPE}} {{MEMBER_NAME}};
{{/IN_PARAMS}}};

template <>
struct OutParam<Querys::{{QUERY_NAME}}> {
{{#OUT_PARAMS}}    {{CPP_TYPE}} {{MEMBER_NAME}};
{{/OUT_PARAMS}}};

template <>
struct TypeConversion<InParam<Querys::{{QUERY_NAME}}>> {
    using values = CMParamList;
    using prm = InParam<Querys::{{QUERY_NAME}}>;
    static auto GetParameters(const prm& m) {
        return std::make_tuple(
{{#IN_PARAMS}}            std::make_pair("{{DB_NAME}}", [&m]() -> std::wstring {
{{#HAS_NULL_VALUE}}                if (m.{{MEMBER_NAME}} == {{NULL_VALUE}}) return L"NULL";
{{/HAS_NULL_VALUE}}                return TC::FormatValue(m.{{MEMBER_NAME}});
            }){{^_LAST}},{{/_LAST}}
{{/IN_PARAMS}}        );
    }
    static void ToParam(const prm &m, values &v) {
        v.SetParams({
{{#IN_PARAMS}}{{#HAS_NULL_VALUE}}            { "{{DB_NAME}}", (m.{{MEMBER_NAME}} == {{NULL_VALUE}}) ? std::any() : std::any(m.{{MEMBER_NAME}}) },
{{/HAS_NULL_VALUE}}{{^HAS_NULL_VALUE}}            { "{{DB_NAME}}", std::any(m.{{MEMBER_NAME}}) },
{{/HAS_NULL_VALUE}}{{/IN_PARAMS}}        });
    }
    static void FromParam(const values &v, prm &m) {
{{#IN_PARAMS}}        v.GetParam("{{DB_NAME}}", m.{{MEMBER_NAME}});
{{/IN_PARAMS}}    }
    static void Reset(prm &m) {
{{#IN_PARAMS}}        m.{{MEMBER_NAME}} = {{DEFAULT_VALUE}};
{{/IN_PARAMS}}    }
    static int getID(const prm &m) { return m.{{IN_KEY_FIELD_NAME}}; }
    static void setID(prm &m, int newID) {
        m.m_iID = newID;
{{#IN_HAS_EXTRA_KEY}}        m.{{IN_KEY_FIELD_NAME}} = newID;
{{/IN_HAS_EXTRA_KEY}}    }
};

template <>
struct TypeConversion<OutParam<Querys::{{QUERY_NAME}}>> {
    using values = CMParamList;
    using prm = OutParam<Querys::{{QUERY_NAME}}>;
    using Getter = std::wstring (*)(const prm&);
    static std::unordered_map<std::wstring, Getter> GetDisplayValues(const prm& m) {
        return {
{{#OUT_PARAMS}}            {L"{{DB_NAME}}", [](const prm& m) { return {{DISPLAY_EXPR}}; }}{{^_LAST}},{{/_LAST}}
{{/OUT_PARAMS}}        };
    }
    template<typename Row>
    static void FromRow(const Row& row, prm &m) {
{{#OUT_PARAMS}}{{FROM_ROW_LINE}}
{{/OUT_PARAMS}}    }
    static void ToParam(const prm &m, values &v) {
        v.SetParams({
{{#OUT_PARAMS}}            { "{{DB_NAME}}", m.{{MEMBER_NAME}} },
{{/OUT_PARAMS}}        });
    }
    static void FromParam(const values &v, prm &m) {
{{#OUT_PARAMS}}        v.GetParam("{{DB_NAME}}", m.{{MEMBER_NAME}});
{{/OUT_PARAMS}}    }
    static void Reset(prm &m) {
{{#OUT_PARAMS}}        m.{{MEMBER_NAME}} = {{DEFAULT_VALUE}};
{{/OUT_PARAMS}}    }
    static int getID(const prm &m) { return m.{{OUT_KEY_FIELD_NAME}}; }
    static void setID(prm &m, int newID) {
        m.m_iID = newID;
{{#OUT_HAS_EXTRA_KEY}}        m.{{OUT_KEY_FIELD_NAME}} = newID;
{{/OUT_HAS_EXTRA_KEY}}    }
{{#HAS_TID}}    static int getTreeID(const prm &m) { return m.{{TID_FIELD_NAME}}; }
{{/HAS_TID}}{{^HAS_TID}}    static int getTreeID(const prm &m) { return -1; }
{{/HAS_TID}}{{#HAS_PID}}    static int getPID(const prm &m) { return m.{{PID_FIELD_NAME}}; }
{{/HAS_PID}}{{^HAS_PID}}    static int getPID(const prm &m) { return -1; }
{{/HAS_PID}}    static std::wstring getIDFieldName() { return L"{{OUT_KEY_FIELD_NAME}}"; }
    static std::wstring getTreeIDFieldName() { return L"{{TID_FIELD_NAME}}"; }
    static std::wstring getPIDFieldName() { return L"{{PID_FIELD_NAME}}"; }
};
)";

//=============================================================================
// 1. TPL_SQL_QUERIES_H — GenerateSQLQueriesHeader → SQLQueries.h
//=============================================================================
static const char* TPL_SQL_QUERIES_H = R"(#pragma once
#include <unordered_map>
#include "SQLQueryStorage.h"
#include <string>

enum class Querys {
    None,
{ENUM_ITEMS}};

///< input, output parameters
template <Querys q>
struct InParam {};

template <Querys q>
struct OutParam {};

inline Querys StringToQuerys(const std::wstring& name) {
{STRING_TO_QUERYS}    return Querys::None;
}

)";

//=============================================================================
// 2. TPL_ASSIGN_COMMON_FIELDS — GenerateAssignCommonFields → AssignCommonFields.h
//=============================================================================
static const char* TPL_ASSIGN_COMMON_FIELDS = R"(// AssignCommonFields.h
#pragma once
#include <type_traits>
#include <algorithm>
#include <iterator>
#include <utility>

#define COMMON_FIELDS_LIST(X) \
{FIELDS_LIST}

// Проверка наличия поля
#define DEFINE_HAS_MEMBER(member)                                          \
template <typename T, typename = std::void_t<>>                            \
struct has_##member : std::false_type {};                                  \
                                                                           \
template <typename T>                                                      \
struct has_##member<T, std::void_t<decltype(std::declval<T>().member)>> : std::true_type {};

// Создаем проверки для всех полей
COMMON_FIELDS_LIST(DEFINE_HAS_MEMBER)

// Безопасное присваивание через указатели на поля
template<typename From, typename To, typename FieldTypeFrom, typename FieldTypeTo>
constexpr void SafeAssign(FieldTypeTo To::*to_field, FieldTypeFrom From::*from_field, const From &from, To &to) {
    if constexpr(std::is_assignable_v<FieldTypeTo &, FieldTypeFrom>) {
        to.*to_field = from.*from_field;
    }
}

// Итоговый макрос для безопасного присваивания полей
#define ASSIGN_IF_EXISTS(field) \
    if constexpr (has_##field<From>::value && has_##field<To>::value) { \
        SafeAssign(&To::field, &From::field, from, to); \
    }

// AssignCommonFields - копирование полей структур с одинаковыми именами
template<typename From, typename To>
void AssignCommonFields(const From &from, To &to) {
    COMMON_FIELDS_LIST(ASSIGN_IF_EXISTS)
}

namespace detail {
template<typename T, typename = void>
struct has_reserve : std::false_type {};

template<typename T>
struct has_reserve<T, std::void_t<decltype(std::declval<T &>().reserve(std::declval<size_t>()))>> : std::true_type {};
}

/// Переносит элементы одного контейнера в другой через AssignCommonFields.
/// Работает только если AssignCommonFields доступен для пары типов.
template<
	typename FromContainer,
	typename ToContainer,
	typename From = typename FromContainer::value_type,
	typename To = typename ToContainer::value_type,
	typename = decltype(AssignCommonFields(std::declval<const From &>(), std::declval<To &>()))
>
void CopyCommonFields(const FromContainer &from, ToContainer &to)
{
	to.clear();
	if constexpr(detail::has_reserve<ToContainer>::value) {
		to.reserve(from.size());
	}

	std::transform(from.begin(), from.end(), std::back_inserter(to), [](const From &src) {
		To dst{};
		AssignCommonFields(src, dst);
		return dst;
	});
}
)";

//=============================================================================
// 5. TPL_MODULE_STRUCTS_H — acAllStructForSectionExecute → {Module}.h
//=============================================================================
static const char* TPL_MODULE_STRUCTS_H = R"(//MAV: Automatically generated header file MERPHelper
//{FILENAME}
#pragma once
#include "SQLQueries.h"
#include "ParamList.h"
#include "TypeConversion.h"
#include "IFieldRowReader.h"
#include "QueryTypes.h"

{STRUCT_CODE}
namespace DB {
{DB_ALIASES}}
)";

//=============================================================================
// 6. TPL_DATA_ACCESS_CPP — GenerateDataAccess → DataAccess.cpp
//=============================================================================
static const char* TPL_DATA_ACCESS_CPP = R"(// DataAccess.cpp — автоматически генерируемый файл (MERPHelper)
// Explicit instantiation для DBSelect/DBExecute
#include "DataAccess.h"
#include "DatabaseConfig.h"
{MODULE_INCLUDES}

static DatabaseImpl* g_db = nullptr;

static DatabaseImpl* GetDB() {
    if (!g_db) {
        static DatabaseImpl instance;
        g_db = &instance;
    }
    return g_db;
}

namespace DB {
    void Init(DatabaseImpl* database) { g_db = database; }
    bool BeginTransaction()    { return GetDB()->BeginTransaction(); }
    bool CommitTransaction()   { return GetDB()->CommitTransaction(); }
    bool RollbackTransaction() { return GetDB()->RollbackTransaction(); }
}

template<Querys Q>
void DBSelect(const InParam<Q>& input, std::vector<OutParam<Q>>& output) {
    GetDB()->LoadSQLData(input, output);
}

template<Querys Q>
OutParam<Q> DBExecute(const InParam<Q>& input) {
    OutParam<Q> result;
    TypeConversion<OutParam<Q>>::Reset(result);
    GetDB()->Execute(input, result);
    return result;
}

// === Explicit instantiation ===
{EXPLICIT_INSTANTIATION}
)";

//=============================================================================
// 4. TPL_FILE_SQL_STORAGE — GenerateFileSql → FileSQLQueryStorage.cpp
//=============================================================================
static const char* TPL_FILE_SQL_STORAGE = R"(#include "SQLQueryStorage.h"
#include "SQLQueries.h"

void load_m_queries(std::unordered_map<Querys, std::wstring> &m_queries, int DbType)
{
switch(DbType)
{
case 0: //mssql
m_queries = {
{MSSQL_ENTRIES}
};
break;
case 1: //pg
m_queries = {
{PG_ENTRIES}
};
break;

}}
)";

//=============================================================================
// 8. TPL_EXAMPLE_DATA — GenerateDataExample → edExample (Scintilla)
//=============================================================================
static const char* TPL_EXAMPLE_DATA_SELECT = R"({IN_TYPE};

{IN_PARAMS_BLOCK}
{CON_TYPE};
{OUT_TYPE};
DB::{METHOD_NAME}::Select(inPrm, outCont);
auto element = {TYPE_NAME}::GetFirstValue(outCont);
if(!outCont.empty() && element.has_value())
    outPrm = element.value();

/// структура выходного параметра
///{
{OUT_FIELDS}///};

)";

static const char* TPL_EXAMPLE_DATA_OTHER = R"({IN_TYPE};

{IN_PARAMS_BLOCK}
{OUT_TYPE};
    outPrm = DB::{METHOD_NAME}::Execute(inPrm);

/// структура выходного параметра
///{
{OUT_FIELDS}///};

)";

//=============================================================================
// 9. TPL_EXAMPLE_GRID — Example_InitGrid → edExample (Scintilla)
//=============================================================================
static const char* TPL_EXAMPLE_GRID = R"(// это добавить в header
#include "GridDataSource.h"
#include "CMGridAdapter.h"

protected:
    const LPCTSTR m_GridName = L"GridName";
    CMGrid {GRID_NAME};
	TGridAdapterPtr {ADAPTER_NAME};
	{TYPE_NAME}::dataset {DATASET_NAME};//возможно есть уже
	std::unique_ptr<{TYPE_NAME}::grid_source> {SOURCE_NAME};

// это добавить в cpp
void MyClass::initGrid()
{
	std::vector<TGridColumnConfig> columns = {
{COLUMNS_BLOCK}    };
	{ADAPTER_NAME} = std::make_shared<CMGridCtrlAdapter>(&{GRID_NAME});
	// сохраняем колонки в реестрв ветка в переменной pGridName
	{ADAPTER_NAME}->SetColumnWidthsHandler([](bool bLoad, LPCTSTR pGridName, float *pWidths, int iCount, float *pDefaultWidths) {
		 g_theApp.HandleGridColSettings(bLoad, pGridName, pWidths, iCount, pDefaultWidths); });
	// --- Группировка по ИмяПоляВКоторомИдентификатор ---
	{ADAPTER_NAME}->SetGroupFieldName(L"ИмяПоляВКоторомИдентификатор");
	{ADAPTER_NAME}->SetGroupHeaderFieldName(L"ИмяПоляТекстИзКоторогоБудетЗкголовкомГруппы");
	{ADAPTER_NAME}->EnableGroupView(true);
	// источник данных для грида
	{SOURCE_NAME} = std::make_unique<{TYPE_NAME}::grid_source>(&{DATASET_NAME}, {ADAPTER_NAME});
	// настройи
	{SOURCE_NAME}->SetGridSettingsName(m_GridName);
	{SOURCE_NAME}->SetColumns(columns);
	{DATASET_NAME}.onRecordScroll.connect(
		[this](std::optional<{TYPE_NAME}::out> record) {
		  //
		});
	{SOURCE_NAME}->SetFilterColumns({ L"dbFieldName", ...});
	{SOURCE_NAME}->Refresh();
};


/* пояснялка для добрых людей
создаем датасет для запроса
	{TYPE_NAME}::dataset {DATASET_NAME};

хотим этот датасет отобразить в гриде
	CMGrid {GRID_NAME};

для грида будем хранить настройки столбцов в
	const LPCTSTR m_GridName = L"GridName";

создаем адаптер для конкретного грида
	TGridAdapterPtr {ADAPTER_NAME}; -- это указатель создадим позже

создаем источник данных. который вытягивет данные из датасета.
	std::unique_ptr<{TYPE_NAME}::grid_source> {SOURCE_NAME};

далее гдето в коде у нас есть создание грида
	initGrid()

описываем столбцы
	std::vector<TGridColumnConfig> columns

созадаем шаредпоинтер для адаптера
	{ADAPTER_NAME} = std::make_shared<CMGridCtrlAdapter>(&{GRID_NAME});

навешиваем на адаптер колбэк для храненя и востановления ширины колонок
	{ADAPTER_NAME}->SetColumnWidthsHandler([](bool bLoad, LPCTSTR pGridName, float *pWidths, int iCount, float *pDefaultWidths) {
		 g_theApp.HandleGridColSettings(bLoad, pGridName, pWidths, iCount, pDefaultWidths); });

если нужна группировка то делаем
	{ADAPTER_NAME}->SetGroupFieldName(L"ИмяПоляВКоторомИдентификатор");
	{ADAPTER_NAME}->SetGroupHeaderFieldName(L"ИмяПоляТекстИзКоторогоБудетЗкголовкомГруппы");
	{ADAPTER_NAME}->EnableGroupView(true);

источник данных для грида. в него передается указатель на датасет и адаптер (шаредпоинтер) грида.
	 {SOURCE_NAME} = std::make_unique<{TYPE_NAME}::grid_source>(&{DATASET_NAME}, {ADAPTER_NAME});

настройи имени настроек
	{SOURCE_NAME}->SetGridSettingsName(m_GridName);

создаем колонки
	{SOURCE_NAME}->SetColumns(columns);

фильтрация идет по всем колонкам но можно выбрать только те которые надо
	{SOURCE_NAME}->SetFilterColumns({ L"FileName", ...});

колбэк на событие сметы записи в датасете! в датсете запись меняется по смене активной строки в гриде сразу
	{DATASET_NAME}.onRecordScroll.connect(
		[this](std::optional<{TYPE_NAME}::out> record) {
		  //
		});
открываем или переоткрываем датасет
	 {SOURCE_NAME}->Refresh();

*/
)";

//=============================================================================
// 10. TPL_EXAMPLE_DATASET — Example_DataSetCpp → edExample (Scintilla)
//=============================================================================
static const char* TPL_EXAMPLE_DATASET = R"(// Генерация Dataset с callback'ами
// В заголовочном файле объявить:
// {TYPE_NAME}::dataset {DATASET_NAME};

void MyClass::Set{METHOD_NAME}CallBack()
{
{ON_OPEN_BLOCK}{SAVE_BLOCK}{DELETE_BLOCK}{RELOAD_BLOCK}    // Валидация каждого поля при его изменении (размер, границы, null)
    {DATASET_NAME}.SetValidateParamCallback([this](const std::string &prm_name, const std::any &value)->bool
    {
        // Проверка размера поля, границ, null и т.д.
        // if(prm_name == "m_sName" && std::any_cast<std::string>(value).empty())
        //     return false; // Имя не может быть пустым
        // if(prm_name == "m_fQuantity" && std::any_cast<float>(value) < 0)
        //     return false; // Количество не может быть отрицательным
        return true;
    });

    // Проверка записи целиком и установка зависимых параметров
    {DATASET_NAME}.SetValidateCommitCallback([this](CMParamList &prm, std::vector<std::string> &fields)->bool
    {
        // for(const auto &field : fields)
        // {
        //     if(field == "m_fPrice") // При изменении цены
        //     {
        //         float price, qty, sum;
        //         prm.GetParam("m_fPrice", price);
        //         prm.GetParam("m_fQuantity", qty);
        //         sum = price * qty;
        //         prm.SetNewParam("m_fSum", sum); // Пересчитать сумму
        //     }
        // }
        return true;
    });
}
)";

static const char* TPL_CALLBACK_OPEN = R"(    // Загрузка данных при открытии датасета
    {DATASET_NAME}.SetOnOpenCallback([this]({TYPE_NAME}::dataset &ds) {
        auto &inParam = ds.GetParams();   // параметры открытия хранятся в датасете для обновления.
        //берем параметры из объекта и запоминаем их
        {TYPE_NAME}::in inPrm = inParam;
{IN_PARAMS_BLOCK}        {TYPE_NAME}::cont outCont;
        {TYPE_NAME}::out outPrm;
        ds.SetParams(inPrm);
        DB::{METHOD_NAME}::Select(inPrm, ds.GetData());
    });

)";

static const char* TPL_CALLBACK_SAVE_HEADER = R"(    // Обработка сохранения записи (INSERT/UPDATE)
    {DATASET_NAME}.SetBeforeSaveCallback([this](CMParamList &prm)
   {
       {TYPE_NAME}::out item;
       {TYPE_NAME}::out_convert::FromParam(prm, item);
       bool InsertNewRecord = (item.{KEY_FIELD} == GLOBAL_INVALID_ID);
       if(!DB::BeginTransaction())
           return false;
)";

static const char* TPL_CALLBACK_SAVE_INSERT = R"(       if(InsertNewRecord)
       {
           {I_IN_TYPE};
           {I_OUT_TYPE};
           AssignCommonFields(item, inPrm);
           inPrm.m_iID = GLOBAL_INVALID_ID;
           outPrm = DB::{I_METHOD_NAME}::Execute(inPrm);
           if(outPrm.m_iID == GLOBAL_INVALID_ID)
           {
               DB::RollbackTransaction();
               AddMessage(L"Ошибка добавления", CMessageStack::MessageType::Error, 0);
               return false;
           }
           else
           {
               {TYPE_NAME}::out_convert::setID(item, outPrm.m_iID);
               {TYPE_NAME}::out_convert::ToParam(item, prm);
           }
       }
)";

static const char* TPL_CALLBACK_SAVE_UPDATE = R"(       {
           {U_IN_TYPE};
           {U_OUT_TYPE};
           AssignCommonFields(item, inPrm);
           outPrm = DB::{U_METHOD_NAME}::Execute(inPrm);
           if(outPrm.m_iID == GLOBAL_INVALID_ID)
           {
               AddMessage(L"Ошибка редактирования", CMessageStack::MessageType::Error, 0);
               return false;
           }
       }
)";

static const char* TPL_CALLBACK_SAVE_RELOAD = R"(       if({DATASET_NAME}.OnReloadRecordByIdExecute(item.{KEY_FIELD}, item))
           {TYPE_NAME}::out_convert::ToParam(item, prm);
)";

static const char* TPL_CALLBACK_SAVE_FOOTER = R"(       return DB::CommitTransaction();
    });

)";

static const char* TPL_CALLBACK_DELETE = R"(    // Удаление записи из базы данных
    {DATASET_NAME}.SetBeforeDeleteCallback([this](const {TYPE_NAME}::out &rec)->bool
    {
        {D_IN_TYPE};
        inPrm.m_iID = rec.{KEY_FIELD};
{D_EXTRA_KEY_FIELDS}        {D_OUT_TYPE};
        outPrm = DB::{D_METHOD_NAME}::Execute(inPrm);
        return outPrm.m_iID != GLOBAL_INVALID_ID;
    });

)";

static const char* TPL_CALLBACK_RELOAD = R"(    // Перезагрузка одной записи по ID после сохранения
    {DATASET_NAME}.SetOnReloadRecordByIdCallback([this](int id, {TYPE_NAME}::out &data)->bool
    {
        bool res = false;
        {O_IN_TYPE};
        inPrm.m_iID = id;
{O_EXTRA_KEY_FIELDS}
        {O_TYPE_NAME}::cont outCont;
        {O_OUT_TYPE};
        DB::{O_METHOD_NAME}::Select(inPrm, outCont);
        auto element = {O_TYPE_NAME}::GetFirstValue(outCont);
        if(!outCont.empty() && element.has_value())
        {
            outPrm = element.value();
            AssignCommonFields(outPrm, data);
            res = true;
        }
        return res;
    });

)";
