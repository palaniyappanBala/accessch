#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "fltstore.h"

RTL_AVL_TABLE FiltersTree::m_Tree;
EX_PUSH_LOCK FiltersTree::m_AccessLock;
LONG FiltersTree::m_FilterIdCounter;

//////////////////////////////////////////////////////////////////////////
typedef struct _ITEM_FILTERS
{
    Interceptors        m_Interceptor;
    DriverOperationId   m_Operation;
    ULONG               m_Minor;
    OperationPoint      m_OperationType;
    
    Filters*            m_Filters;
} ITEM_FILTERS, *PITEM_FILTERS;
//////////////////////////////////////////////////////////////////////////

Filters::Filters (
    )
{
    ExInitializeRundownProtection( &m_Ref );
    FltInitializePushLock( &m_AccessLock );
    
    RtlInitializeBitMap (
        &m_ActiveFilters,
        m_ActiveFiltersBuffer,
        NumberOfBits
        );
    
    RtlClearAllBits( &m_ActiveFilters );

    m_FilterCount = 0;
    InitializeListHead( &m_FilterEntryList );
    InitializeListHead( &m_ParamsCheckList );
}

Filters::~Filters (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );

    ExRundownCompleted( &m_Ref );
    FltDeletePushLock( &m_AccessLock);

    FilterEntry* pItem;

    if ( !IsListEmpty( &m_FilterEntryList ) )
    {
        PLIST_ENTRY Flink;

        Flink = m_FilterEntryList.Flink;

        while ( Flink != &m_FilterEntryList )
        {
            pItem = CONTAINING_RECORD( Flink, FilterEntry, m_List );
            Flink = Flink->Flink;

            m_FilterCount--;
            RemoveEntryList( &pItem->m_List ); //is it neccessary?

            FREE_POOL( pItem );
        }
    }
}

NTSTATUS
Filters::AddRef (
    )
{
    NTSTATUS status = ExAcquireRundownProtection( &m_Ref );
    return status;
}

void
Filters::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}

VERDICT
Filters::GetVerdict (
    __in EventData *Event,
    __out PARAMS_MASK *ParamsMask
    )
{
    ASSERT( ARGUMENT_PRESENT( Event ) );
    ASSERT( ARGUMENT_PRESENT( ParamsMask ) );
    
    FltAcquirePushLockShared( &m_AccessLock );

    if ( !IsListEmpty( &m_ParamsCheckList ) )
    {
        PLIST_ENTRY Flink = m_ParamsCheckList.Flink;
        while ( Flink != &m_ParamsCheckList )
        {
            ParamCheckEntry* pEntry = CONTAINING_RECORD (
                Flink,
                ParamCheckEntry,
                m_List
                );

            Flink = Flink->Flink;

            // \todo pEntry->m_NumbersCount
        }

    }

    // \todo enum in m_FilteringHead and gather filters matched event

    FltReleasePushLock( &m_AccessLock );

    return VERDICT_NOT_FILTERED;
}

__checkReturn
NTSTATUS
Filters::AddFilter (
    __in_opt ULONG RequestTimeout,
    __in PARAMS_MASK WishMask,
    __in ULONG ParamsCount,
    __in PPARAM_ENTRY Params,
    __out PULONG FilterId
    )
{
    ASSERT( ARGUMENT_PRESENT( Params ) );

    FilterEntry* pEntry = (FilterEntry*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( FilterEntry ),
        _ALLOC_TAG
        );

    if( !pEntry )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pEntry->m_FilterId = FiltersTree::GetNextFilterid();
    *FilterId = pEntry->m_FilterId;
    pEntry->m_RequestTimeout = RequestTimeout;
    pEntry->m_WishMask = WishMask;

    FltAcquirePushLockExclusive( &m_AccessLock );

    m_FilterCount++;

    InsertTailList( &m_FilterEntryList, &pEntry->m_List );

    FltReleasePushLock( &m_AccessLock );

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////

VOID
FiltersTree::Initialize (
    )
{
    FltInitializePushLock( &m_AccessLock );

    RtlInitializeGenericTableAvl (
        &m_Tree,
        FiltersTree::Compare,
        FiltersTree::Allocate,
        FiltersTree::Free,
        NULL
        );

    m_FilterIdCounter = 0;
}


VOID
FiltersTree::Destroy (
    )
{
    FltDeletePushLock( &m_AccessLock );
}

RTL_GENERIC_COMPARE_RESULTS
NTAPI
FiltersTree::Compare (
    __in struct _RTL_AVL_TABLE *Table,
    __in PVOID FirstStruct,
    __in PVOID SecondStruct
    )
{
    UNREFERENCED_PARAMETER( Table );

    RTL_GENERIC_COMPARE_RESULTS result;

    PITEM_FILTERS Struct1 = (PITEM_FILTERS) FirstStruct;
    PITEM_FILTERS Struct2 = (PITEM_FILTERS) SecondStruct;

    int ires = memcmp (
        Struct1,
        Struct2,
        sizeof( ITEM_FILTERS ) - FIELD_OFFSET( ITEM_FILTERS, m_Filters )
        );

    switch ( ires )
    {
    case 0:
        result = GenericEqual;
        break;

    case 1:
        result = GenericGreaterThan;
        break;

    case -1:
        result = GenericLessThan;
        break;

    default:
        __debugbreak();
        break;
    }

    return result;
}

PVOID
NTAPI
FiltersTree::Allocate (
    __in struct _RTL_AVL_TABLE *Table,
    __in CLONG ByteSize
    )
{
    UNREFERENCED_PARAMETER( Table );

    PVOID ptr = ExAllocatePoolWithTag (
        PagedPool,
        ByteSize,
        _ALLOC_TAG
        );
    
    return ptr;
}

VOID
NTAPI
FiltersTree::Free (
    __in struct _RTL_AVL_TABLE *Table,
    __in __drv_freesMem(Mem) __post_invalid PVOID Buffer
    )
{
    UNREFERENCED_PARAMETER( Table );

    FREE_POOL( Buffer );
}

FiltersTree::FiltersTree (
    )
{

}

FiltersTree::~FiltersTree (
    )
{
    // \todo free tree items
}

VOID
FiltersTree::DeleteAllFilters (
    )
{
    PITEM_FILTERS pItem;

    do 
    {
        pItem = (PITEM_FILTERS) RtlEnumerateGenericTableAvl (
            &m_Tree,
            TRUE
            );

        if ( pItem )
        {
            ASSERT( pItem->m_Filters );
            pItem->m_Filters->Filters::~Filters();
            FREE_POOL( pItem->m_Filters );

            RtlDeleteElementGenericTableAvl( &m_Tree, pItem );
        }

    } while ( pItem );
}

LONG
FiltersTree::GetNextFilterid (
    )
{
    LONG result = InterlockedIncrement( &m_FilterIdCounter );

    return result;
}

__checkReturn
Filters*
FiltersTree::GetFiltersBy (
    __in Interceptors Interceptor,
    __in DriverOperationId Operation,
    __in_opt ULONG Minor,
    __in OperationPoint OperationType
    )
{
    Filters* pFilters = NULL;

    ITEM_FILTERS item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;

    FltAcquirePushLockShared( &m_AccessLock );

    PITEM_FILTERS pItem = (PITEM_FILTERS) RtlLookupElementGenericTableAvl (
        &m_Tree,
        &item
        );

    if ( pItem)
    {
        pFilters = pItem->m_Filters;

        NTSTATUS status = pFilters->AddRef();
        if ( !NT_SUCCESS( status ) )
        {
            pFilters = NULL;
        }

    }

    FltReleasePushLock( &m_AccessLock );

    return pFilters;
}

__checkReturn
Filters*
FiltersTree::GetOrCreateFiltersBy (
    __in Interceptors Interceptor,
    __in DriverOperationId Operation,
    __in_opt ULONG Minor,
    __in OperationPoint OperationType
    )
{
    Filters* pFilters = NULL;

    ITEM_FILTERS item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;
    
    BOOLEAN newElement;
    
    FltAcquirePushLockExclusive( &m_AccessLock );

    PITEM_FILTERS pItem = (PITEM_FILTERS) RtlInsertElementGenericTableAvl (
        &m_Tree,
        &item,
        sizeof( item ),
        &newElement
        );
    
    if ( pItem)
    {
        if ( newElement )
        {
            pItem->m_Filters = (Filters*) ExAllocatePoolWithTag (
                PagedPool,
                sizeof(Filters),
                _ALLOC_TAG
                );
        
            if ( pItem->m_Filters )
            {
                pFilters = pItem->m_Filters;
                pFilters->Filters::Filters();
            }
        }
        else
        {
            pFilters = pItem->m_Filters;
        }
    }

    if ( pFilters )
    {
        NTSTATUS status = pFilters->AddRef();
        if ( !NT_SUCCESS( status ) )
        {
            pFilters = NULL;
        }
    }

    FltReleasePushLock( &m_AccessLock );

    return pFilters;
}