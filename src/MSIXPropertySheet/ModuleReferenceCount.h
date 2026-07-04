// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

class ModuleReferenceCount
{
public:
    inline static LONG Increment()
    {
        return ::InterlockedIncrement(&s_referenceCount);
    }

    inline static LONG Decrement()
    {
        return ::InterlockedDecrement(&s_referenceCount);
    }

    inline static bool CanUnloadNow()
    {
        return s_referenceCount > 0 ? S_FALSE : S_OK;
    }

private:
    inline static volatile LONG s_referenceCount{};
};
