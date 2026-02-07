//---------------------------------------------------------------------------
// uEventStore.h — Event data storage (header-only)
//
// Owns the TEventData struct and TEventStore class.
// Extracted from TfrmMain to follow Single Responsibility Principle.
//---------------------------------------------------------------------------

#ifndef uEventStoreH
#define uEventStoreH

//---------------------------------------------------------------------------
#include <System.SysUtils.hpp>
#include <vector>

//---------------------------------------------------------------------------
// Event data structure
//---------------------------------------------------------------------------
struct TEventData
{
    UnicodeString Time;
    UnicodeString Type;
    UnicodeString Data;
    // Extended fields for detailed tool info
    UnicodeString ToolInput;   // JSON input for tool_start/tool_end
    UnicodeString ToolOutput;  // JSON output for tool_end
    UnicodeString ToolUseId;   // Tool use ID
    UnicodeString RequestId;   // Permission request ID
    int DurationMs = 0;        // Duration for tool_end
};

//---------------------------------------------------------------------------
// TEventStore — stores and retrieves TEventData items
//---------------------------------------------------------------------------
class TEventStore
{
public:
    void Add(const TEventData &ev)
    {
        FItems.push_back(ev);
    }

    const TEventData& Get(int index) const
    {
        return FItems.at(index);
    }

    TEventData GetSafe(int index) const
    {
        if (index >= 0 && index < (int)FItems.size())
            return FItems[index];
        return TEventData();
    }

    std::vector<TEventData> GetRange(int limit, int offset) const
    {
        std::vector<TEventData> result;
        int count = (int)FItems.size();
        int start = (offset < 0) ? 0 : offset;
        if (start >= count) return result;
        int end = count;
        if (limit > 0 && start + limit < end)
            end = start + limit;
        for (int i = start; i < end; i++)
            result.push_back(FItems[i]);
        return result;
    }

    int Count() const { return (int)FItems.size(); }

    void Clear() { FItems.clear(); }

private:
    std::vector<TEventData> FItems;
};

//---------------------------------------------------------------------------
#endif
