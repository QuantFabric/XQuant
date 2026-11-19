#ifndef XQUANT_HASH_KEYS_HPP
#define XQUANT_HASH_KEYS_HPP

#include <cstddef>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "phmap.h"

namespace XQuant
{

struct AccountTickerKey
{
    char Account[16];
    char Ticker[20];
};

struct AccountOrderRefKey
{
    char Account[16];
    char OrderRef[32];
};

static_assert(std::is_trivial<AccountTickerKey>::value,
              "AccountTickerKey must be trivial");
static_assert(std::is_standard_layout<AccountTickerKey>::value,
              "AccountTickerKey must be standard layout");
static_assert(std::is_trivial<AccountOrderRefKey>::value,
              "AccountOrderRefKey must be trivial");
static_assert(std::is_standard_layout<AccountOrderRefKey>::value,
              "AccountOrderRefKey must be standard layout");

inline void CopyKeyPart(char* destination, std::size_t capacity, const char* source)
{
    if(source == nullptr || capacity == 0)
    {
        return;
    }

    std::size_t length = 0;
    while(length + 1 < capacity && source[length] != '\0')
    {
        ++length;
    }
    std::memcpy(destination, source, length);
    destination[length] = '\0';
}

inline AccountTickerKey MakeAccountTickerKey(const char* account, const char* ticker)
{
    AccountTickerKey key{};
    CopyKeyPart(key.Account, sizeof(key.Account), account);
    CopyKeyPart(key.Ticker, sizeof(key.Ticker), ticker);
    return key;
}

inline AccountOrderRefKey MakeAccountOrderRefKey(const char* account, const char* orderRef)
{
    AccountOrderRefKey key{};
    CopyKeyPart(key.Account, sizeof(key.Account), account);
    CopyKeyPart(key.OrderRef, sizeof(key.OrderRef), orderRef);
    return key;
}

struct AccountTickerKeyHash
{
    std::size_t operator()(const AccountTickerKey& key) const noexcept
    {
        return phmap::HashState().combine(
            0, std::string_view(key.Account), std::string_view(key.Ticker));
    }
};

struct AccountTickerKeyEqual
{
    bool operator()(const AccountTickerKey& lhs, const AccountTickerKey& rhs) const noexcept
    {
        return std::memcmp(lhs.Account, rhs.Account, sizeof(lhs.Account)) == 0 &&
               std::memcmp(lhs.Ticker, rhs.Ticker, sizeof(lhs.Ticker)) == 0;
    }
};

struct AccountOrderRefKeyHash
{
    std::size_t operator()(const AccountOrderRefKey& key) const noexcept
    {
        return phmap::HashState().combine(
            0, std::string_view(key.Account), std::string_view(key.OrderRef));
    }
};

struct AccountOrderRefKeyEqual
{
    bool operator()(const AccountOrderRefKey& lhs, const AccountOrderRefKey& rhs) const noexcept
    {
        return std::memcmp(lhs.Account, rhs.Account, sizeof(lhs.Account)) == 0 &&
               std::memcmp(lhs.OrderRef, rhs.OrderRef, sizeof(lhs.OrderRef)) == 0;
    }
};

} // namespace XQuant

#endif // XQUANT_HASH_KEYS_HPP
