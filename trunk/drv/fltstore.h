#ifndef __fltstore_h
#define __fltstore_h

// used for filterid and groupid
#define NumberOfBits 256
#define BitMapBufferSizeInUlong (NumberOfBits / 32)

typedef struct _FltData
{
    ULONG               m_DataSize;
    ULONG               m_Count;
    UCHAR               m_Data[1];
} FltData;

#define PosListItemType  ULONG

typedef struct _ParamCheckEntry
{
    LIST_ENTRY          m_List;
    Parameters          m_Parameter;
    FltOperation        m_Operation;
    ULONG               m_Flags;
    ULONG               m_PosCount;
    PosListItemType*    m_FilterPosList;
    FltData             m_Data;
} ParamCheckEntry;

typedef struct _FilterEntry
{
    ULONG               m_Flags;
    ULONG               m_FilterId;
    UCHAR               m_GroupId;
    VERDICT             m_Verdict;
    HANDLE              m_ProcessId;
    PARAMS_MASK         m_WishMask;
    ULONG               m_RequestTimeout;
} FilterEntry;

//////////////////////////////////////////////////////////////////////////
class Filters
{
public:
    Filters();
    ~Filters();

    NTSTATUS
    AddRef (
        );

    void
    Release();

    BOOLEAN
    IsEmpty();

    VERDICT
    GetVerdict (
        __in EventData *Event,
        __out PARAMS_MASK *ParamsMask
        );
    
    NTSTATUS
    AddFilter (
        __in UCHAR GroupId,
        __in VERDICT Verdict,
        __in HANDLE ProcessId,
        __in_opt ULONG RequestTimeout,
        __in PARAMS_MASK WishMask,
        __in_opt ULONG ParamsCount,
        __in PPARAM_ENTRY Params,
        __out PULONG FilterId
        );

    ULONG
    CleanupByProcess (
        __in HANDLE ProcessId
        );

private:
    __checkReturn
    NTSTATUS
    ParseParamsUnsafe (
        __in ULONG Position,
        __in ULONG ParamsCount,
        __in PPARAM_ENTRY Params
        );

    __checkReturn
    NTSTATUS
    GetFilterPosUnsafe (
    PULONG Position
        );

    __checkReturn
    NTSTATUS
    TryToFindExisting (
        __in PPARAM_ENTRY ParamEntry,
        __in ULONG Position,
        __deref_out_opt ParamCheckEntry** Entry
        );

    ParamCheckEntry*
    AddParameterWithFilterPos (
        __in PPARAM_ENTRY ParamEntry,
        __in ULONG Position
        );

    void
    DeleteParamsByFilterPosUnsafe (
        __in_opt ULONG Position
        );

    void
    MoveFilterPosInParams (
        ULONG IdxFrom,
        ULONG IdxTo
        );

    NTSTATUS
    CheckSingleEntryUnsafe (
        __in ParamCheckEntry* Entry,
        __in EventData *Event
        );

    NTSTATUS
    CheckParamsList (
        __in EventData *Event,
        __in PULONG Unmatched,
        __in PRTL_BITMAP Filtersbitmap
        );

private:
    static ULONG        m_AllocTag;

    EX_RUNDOWN_REF      m_Ref;
    EX_PUSH_LOCK        m_AccessLock;

    RTL_BITMAP          m_GroupsMap;
    ULONG               m_GroupsMapBuffer[ BitMapBufferSizeInUlong ];
    ULONG               m_GroupCount;

    RTL_BITMAP          m_ActiveFilters;
    ULONG               m_ActiveFiltersBuffer[ BitMapBufferSizeInUlong ];
    ULONG               m_FiltersCount;
    FilterEntry*        m_FiltersArray;
    LIST_ENTRY          m_ParamsCheckList;
};

//////////////////////////////////////////////////////////////////////////

class FiltersTree
{
public:
    static
    void
    Initialize (
        );

    static
    void
    Destroy (
        );

    static
    void
    DeleteAllFilters (
        );

    static
    void
    CleanupFiltersByPid (
        __in HANDLE ProcessId
        );

    static LONG GetNextFilterid();
    static LONG GetCount();
    static BOOLEAN IsActive();
    static NTSTATUS ChangeState (
        __in_opt BOOLEAN Activate
        );

    static RTL_AVL_COMPARE_ROUTINE Compare;
    static RTL_AVL_ALLOCATE_ROUTINE Allocate;
    static RTL_AVL_FREE_ROUTINE Free;
  
    __checkReturn
    static
    Filters*
    GetFiltersBy (
        __in Interceptors Interceptor,
        __in DriverOperationId Operation,
        __in_opt ULONG Minor,
        __in OperationPoint OperationType
        );
    
    __checkReturn
    static
    Filters*
    GetOrCreateFiltersBy (
        __in Interceptors Interceptor,
        __in DriverOperationId Operation,
        __in_opt ULONG Minor,
        __in OperationPoint OperationType
        );
    
private:
    static ULONG            m_AllocTag;
    static RTL_AVL_TABLE    m_Tree;
    static EX_PUSH_LOCK     m_AccessLock;
    
    /// \todo clear counter when disconnected
    static LONG             m_FilterIdCounter;

public:
    FiltersTree();
    ~FiltersTree();

    static LONG             m_Count;
    static LONG             m_Flags;
};


#endif //__fltstore_h