#include <string>
#include <vector>
#include <base/find_symbols.h>
#include <gtest/gtest.h>


template <char ... symbols>
void test_find_first_not(const std::string & haystack, std::size_t expected_pos)
{
    const char * begin = haystack.data();
    const char * end = haystack.data() + haystack.size();

    ASSERT_EQ(begin + expected_pos, find_first_not_symbols<symbols...>(begin, end));
}

void test_find_first_not(const std::string & haystack, const std::string & symbols, const std::size_t expected_pos)
{
    const char * begin = haystack.data();

    ASSERT_EQ(begin + expected_pos, find_first_not_symbols(haystack, symbols));
}


TEST(FindSymbols, SimpleTest)
{
    const std::string s = "Hello, world! Goodbye...";
    const char * begin = s.data();
    const char * end = s.data() + s.size();

    ASSERT_EQ(find_first_symbols<'a'>(begin, end), end);
    ASSERT_EQ(find_first_symbols<'e'>(begin, end), begin + 1);
    ASSERT_EQ(find_first_symbols<'.'>(begin, end), begin + 21);
    ASSERT_EQ(find_first_symbols<' '>(begin, end), begin + 6);
    ASSERT_EQ(find_first_symbols<'H'>(begin, end), begin);
    ASSERT_EQ((find_first_symbols<'a', 'e'>(begin, end)), begin + 1);

    ASSERT_EQ((find_first_symbols<'a', 'e', 'w', 'x', 'z'>(begin, end)), begin + 1);
    ASSERT_EQ((find_first_symbols<'p', 'q', 's', 'x', 'z'>(begin, end)), end);

    ASSERT_EQ(find_last_symbols_or_null<'a'>(begin, end), nullptr);
    ASSERT_EQ(find_last_symbols_or_null<'e'>(begin, end), end - 4);
    ASSERT_EQ(find_last_symbols_or_null<'.'>(begin, end), end - 1);
    ASSERT_EQ(find_last_symbols_or_null<' '>(begin, end), end - 11);
    ASSERT_EQ(find_last_symbols_or_null<'H'>(begin, end), begin);
    ASSERT_EQ((find_last_symbols_or_null<'a', 'e'>(begin, end)), end - 4);

    {
        std::vector<std::string> vals;
        splitInto<' ', ','>(vals, "hello, world", true);
        ASSERT_EQ(vals, (std::vector<std::string>{"hello", "world"}));
    }

    {
        std::vector<std::string> vals;
        splitInto<' ', ','>(vals, "s String", true);
        ASSERT_EQ(vals, (std::vector<std::string>{"s", "String"}));
    }
}

template <bool positive, detail::ReturnMode return_mode>
inline const char * find_first_symbols_sse42_MY(const char * const begin, const char * const end, const char * symbols, size_t num_chars)
{
    using namespace detail;
    const char * pos = begin;

#if defined(__SSE4_2__)
    constexpr int mode = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_LEAST_SIGNIFICANT;

#if defined(__AVX512F__) || defined(__AVX512BW__) || defined(__AVX__) || defined(__AVX2__)

#else
    // This is to avoid read past end of allocated string while loading `set` from `symbols` if `num_chars < 16`.
    char buffer[16] = {'\0'};
    memcpy(buffer, symbols, num_chars);
    const __m128i set = _mm_loadu_si128(reinterpret_cast<const __m128i *>(buffer));
#endif

    for (; pos + 15 < end; pos += 16)
    {
        __m128i bytes = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pos));

        if constexpr (positive)
        {
            if (_mm_cmpestrc(set, num_chars, bytes, 16, mode))
                return pos + _mm_cmpestri(set, num_chars, bytes, 16, mode);
        }
        else
        {
            if (_mm_cmpestrc(set, num_chars, bytes, 16, mode | _SIDD_NEGATIVE_POLARITY))
                return pos + _mm_cmpestri(set, num_chars, bytes, 16, mode | _SIDD_NEGATIVE_POLARITY);
        }
    }
#endif

    for (; pos < end; ++pos)
        if (maybe_negate<positive>(is_in(*pos, symbols, num_chars)))
            return pos;

    return return_mode == ReturnMode::End ? end : nullptr;
}

template <char... symbols>
inline const char * find_first_symbols_MY(const char * begin, const char * end)
{
    return detail::find_first_symbols_dispatch<true, detail::ReturnMode::End, symbols...>(begin, end);
}

TEST(FindSymbols, RunTimeNeedle)
{
    auto test_haystack = [](const auto & haystack, const auto & unfindable_needle) {
#define TEST_HAYSTACK_AND_NEEDLE(haystack_, needle_) \
        do { \
            const auto & h = haystack_; \
            const auto & n = needle_; \
            EXPECT_EQ( \
                    std::find_first_of(h.data(), h.data() + h.size(), n.data(), n.data() + n.size()), \
                    find_first_symbols(h, n) \
            ) << "haystack: \"" << h << "\" (" << static_cast<const void*>(h.data()) << ")" \
              << ", needle: \"" << n << "\""; \
        } \
        while (false)

        // can't find needle
        TEST_HAYSTACK_AND_NEEDLE(haystack, unfindable_needle);

#define test_with_modified_needle(haystack, in_needle, needle_update_statement) \
        do \
        { \
            std::string needle = (in_needle); \
            (needle_update_statement); \
            std::cerr << "!!!\tHaystack: \"" << haystack << "\" needele: \"" << needle << "\"" << std::endl; \
            TEST_HAYSTACK_AND_NEEDLE(haystack, needle); \
        } \
        while (false)

        // findable symbol is at beginnig of the needle
        // Can find at first pos of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle.front() = haystack.front());
        // Can find at first pos of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle.front() = haystack.back());
        // Can find in the middle of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle.front() = haystack[haystack.size() / 2]);

        // findable symbol is at end of the needle
        // Can find at first pos of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle.back() = haystack.front());
        // Can find at first pos of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle.back() = haystack.back());
        // Can find in the middle of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle.back() = haystack[haystack.size() / 2]);

        // findable symbol is in the middle of the needle
        // Can find at first pos of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle[needle.size() / 2] = haystack.front());
        // Can find at first pos of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle[needle.size() / 2] = haystack.back());
        // Can find in the middle of haystack
        test_with_modified_needle(haystack, unfindable_needle, needle[needle.size() / 2] = haystack[haystack.size() / 2]);
    };

    // there are 4 major groups of cases:
    // haystack < 16 bytes, haystack > 16 bytes
    // needle < 5 bytes,    needle >= 5 bytes

    // First and last symbols of haystack should be unique
    std::string long_haystack = "Hello, world! Goodbye...?";
    std::string short_haystack = "Hello, world!";

    // In sync with find_first_symbols_dispatch code: long needles receve special treatment.
    // as of now "long" means >= 5
    std::string unfindable_long_needle = "0123456789ABCDEF";
    std::string unfindable_short_needle = "0123";


    for (size_t i = 0; i < 100; ++i)
    {
        {
            SCOPED_TRACE("Long haystack");
            test_haystack(long_haystack, unfindable_long_needle);
            test_haystack(long_haystack, unfindable_short_needle);
        }

        {
            SCOPED_TRACE("Short haystack");
            test_haystack(short_haystack, unfindable_long_needle);
            test_haystack(short_haystack, unfindable_short_needle);
        }

        // re-allocate buffers to make sure that we run tests against different addresses each time.
        long_haystack.reserve(long_haystack.capacity() + 1);
        short_haystack.reserve(long_haystack.capacity() + 1);
        unfindable_long_needle.reserve(unfindable_long_needle.capacity() + 1);
        unfindable_short_needle.reserve(unfindable_short_needle.capacity() + 1);
    }

    // Check that nothing matches on big haystack,
    EXPECT_EQ(find_first_symbols(long_haystack, "ABCDEFIJKLMNOPQRSTUVWXYZacfghijkmnpqstuvxz"), long_haystack.data() + long_haystack.size());

    // only 16 bytes of haystack are checked, so nothing is found
    EXPECT_EQ(find_first_symbols(long_haystack, "ABCDEFIJKLMNOPQR0helloworld"), long_haystack.data() + long_haystack.size());
}

TEST(FindNotSymbols, AllSymbolsPresent)
{
    std::string str_with_17_bytes = "hello world hello";
    std::string str_with_16_bytes = {str_with_17_bytes.begin(), str_with_17_bytes.end() - 1u};
    std::string str_with_15_bytes = {str_with_16_bytes.begin(), str_with_16_bytes.end() - 1u};

    /*
     * The below variations will choose different implementation strategies:
     * 1. Loop method only because it does not contain enough bytes for SSE 4.2
     * 2. SSE4.2 only since string contains exactly 16 bytes
     * 3. SSE4.2 + Loop method will take place because only first 16 bytes are treated by SSE 4.2 and remaining bytes is treated by loop
     *
     * Below code asserts that all calls return the ::end of the input string. This was not true prior to this fix as mentioned in PR #47304
     * */

    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(str_with_15_bytes, str_with_15_bytes.size());
    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(str_with_16_bytes, str_with_16_bytes.size());
    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(str_with_17_bytes, str_with_17_bytes.size());

    const auto * symbols = "helowrd ";

    test_find_first_not(str_with_15_bytes, symbols, str_with_15_bytes.size());
    test_find_first_not(str_with_16_bytes, symbols, str_with_16_bytes.size());
    test_find_first_not(str_with_17_bytes, symbols, str_with_17_bytes.size());
}

TEST(FindNotSymbols, NoSymbolsMatch)
{
    std::string s = "abcdefg";

    // begin should be returned since the first character of the string does not match any of the below symbols
    test_find_first_not<'h', 'i', 'j'>(s, 0u);
    test_find_first_not(s, "hij",0u);
}

TEST(FindNotSymbols, ExtraSymbols)
{
    std::string s = "hello_world_hello";
    test_find_first_not<'h', 'e', 'l', 'o', ' '>(s, 5u);
    test_find_first_not(s, "helo ",5u);
}

TEST(FindNotSymbols, EmptyString)
{
    std::string s;
    test_find_first_not<'h', 'e', 'l', 'o', 'w', 'r', 'd', ' '>(s, s.size());
    test_find_first_not(s, "helowrd ", s.size());
}

TEST(FindNotSymbols, SingleChar)
{
    std::string s = "a";
    test_find_first_not<'a'>(s, s.size());
    test_find_first_not(s, "a", s.size());
}

TEST(FindNotSymbols, NullCharacter)
{
    // special test to ensure only the passed template arguments are used as needles
    // since current find_first_symbols implementation takes in 16 characters and defaults
    // to \0.
    std::string s("abcdefg\0x", 9u);
    test_find_first_not<'a', 'b', 'c', 'd', 'e', 'f', 'g'>(s, 7u);
    test_find_first_not(s, "abcdefg", 7u);
}
