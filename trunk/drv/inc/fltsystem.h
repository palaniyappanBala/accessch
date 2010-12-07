#pragma once
#include  "../../inc/filters.h"
#include  "../inc/fltevents.h"
#include  "../inc/fltstorage.h"

class FilteringSystem
{
public:
    static ULONG        m_AllocTag;

public:
    FilteringSystem();
    ~FilteringSystem();

    __checkReturn
    NTSTATUS
    Attach (
        );

    void
    Detach (
        );

    __checkReturn
    NTSTATUS
    FilterEvent (
        __in PEventData event,
        __in PVERDICT Verdict,
        __in PPARAMS_MASK ParamsMask
        );

private:
    EX_PUSH_LOCK        m_AccessLock;
    LIST_ENTRY          m_List;
};

#define PFilteringSystem FilteringSystem* 