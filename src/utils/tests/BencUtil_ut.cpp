/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "BencUtil.h"

// must be last due to assert() over-write
#include "UtAssert.h"

// define for testing the encoding of a very large tree
// #define ENABLE_BENC_STRESS_TEST

static void BencTestSerialization(BencObj *obj, const char *dataOrig)
{
    ScopedMem<char> data(obj->Encode());
    utassert(data);
    utassert(str::Eq(data, dataOrig));
}

static void BencTestRoundtrip(BencObj *obj)
{
    ScopedMem<char> encoded(obj->Encode());
    utassert(encoded);
    size_t len;
    BencObj *obj2 = BencObj::Decode(encoded, &len);
    utassert(obj2 && len == str::Len(encoded));
    ScopedMem<char> roundtrip(obj2->Encode());
    utassert(str::Eq(encoded, roundtrip));
    delete obj2;
}

static void BencTestParseInt()
{
    struct {
        const char *    benc;
        bool            valid;
        int64_t         value;
    } testData[] = {
        { NULL, false },
        { "", false },
        { "a", false },
        { "0", false },
        { "i", false },
        { "ie", false },
        { "i0", false },
        { "i1", false },
        { "i23", false },
        { "i-", false },
        { "i-e", false },
        { "i-0e", false },
        { "i23f", false },
        { "i2-3e", false },
        { "i23-e", false },
        { "i041e", false },
        { "i9223372036854775808e", false },
        { "i-9223372036854775809e", false },

        { "i0e", true, 0 },
        { "i1e", true, 1 },
        { "i9823e", true, 9823 },
        { "i-1e", true, -1 },
        { "i-53e", true, -53 },
        { "i123e", true, 123 },
        { "i2147483647e", true, INT_MAX },
        { "i2147483648e", true, (int64_t)INT_MAX + 1 },
        { "i-2147483648e", true, INT_MIN },
        { "i-2147483649e", true, (int64_t)INT_MIN - 1 },
        { "i9223372036854775807e", true, _I64_MAX },
        { "i-9223372036854775808e", true, _I64_MIN },
    };

    for (int i = 0; i < dimof(testData); i++) {
        BencObj *obj = BencObj::Decode(testData[i].benc);
        if (testData[i].valid) {
            utassert(obj);
            utassert(obj->Type() == BT_INT);
            utassert(static_cast<BencInt *>(obj)->Value() == testData[i].value);
            BencTestSerialization(obj, testData[i].benc);
            delete obj;
        } else {
            utassert(!obj);
        }
    }
}

static void BencTestParseString()
{
    struct {
        const char *    benc;
        WCHAR *         value;
    } testData[] = {
        { NULL, NULL },
        { "", NULL },
        { "0", NULL },
        { "1234", NULL },
        { "a", NULL },
        { ":", NULL },
        { ":z", NULL },
        { "1:ab", NULL },
        { "3:ab", NULL },
        { "-2:ab", NULL },
        { "2e:ab", NULL },

        { "0:", L"" },
        { "1:a", L"a" },
        { "2::a", L":a" },
        { "4:spam", L"spam" },
        { "4:i23e", L"i23e" },
        { "5:\xC3\xA4\xE2\x82\xAC", L"\u00E4\u20AC" },
    };

    for (int i = 0; i < dimof(testData); i++) {
        BencObj *obj = BencObj::Decode(testData[i].benc);
        if (testData[i].value) {
            utassert(obj);
            utassert(obj->Type() == BT_STRING);
            ScopedMem<WCHAR> value(static_cast<BencString *>(obj)->Value());
            utassert(str::Eq(value, testData[i].value));
            BencTestSerialization(obj, testData[i].benc);
            delete obj;
        } else {
            utassert(!obj);
        }
    }
}

static void BencTestParseRawStrings()
{
    BencArray array;
    array.AddRaw("a\x82");
    array.AddRaw("a\x82", 1);
    BencString *raw = array.GetString(0);
    utassert(raw && str::Eq(raw->RawValue(), "a\x82"));
    BencTestSerialization(raw, "2:a\x82");
    raw = array.GetString(1);
    utassert(raw && str::Eq(raw->RawValue(), "a"));
    BencTestSerialization(raw, "1:a");

    BencDict dict;
    dict.AddRaw("1", "a\x82");
    dict.AddRaw("2", "a\x82", 1);
    raw = dict.GetString("1");
    utassert(raw && str::Eq(raw->RawValue(), "a\x82"));
    BencTestSerialization(raw, "2:a\x82");
    raw = dict.GetString("2");
    utassert(raw && str::Eq(raw->RawValue(), "a"));
    BencTestSerialization(raw, "1:a");
}

static void BencTestParseArray(const char *benc, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(benc);
    utassert(obj);
    utassert(obj->Type() == BT_ARRAY);
    utassert(static_cast<BencArray *>(obj)->Length() == expectedLen);
    BencTestSerialization(obj, benc);
    delete obj;
}

static void BencTestParseArrays()
{
    BencObj *obj;

    obj = BencObj::Decode("l");
    utassert(!obj);
    obj = BencObj::Decode("l123");
    utassert(!obj);
    obj = BencObj::Decode("li12e");
    utassert(!obj);
    obj = BencObj::Decode("l2:ie");
    utassert(!obj);

    BencTestParseArray("le", 0);
    BencTestParseArray("li35ee", 1);
    BencTestParseArray("llleee", 1);
    BencTestParseArray("li35ei-23e2:abe", 3);
    BencTestParseArray("li42e2:teldeedee", 4);
}

static void BencTestParseDict(const char *benc, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(benc);
    utassert(obj);
    utassert(obj->Type() == BT_DICT);
    utassert(static_cast<BencDict *>(obj)->Length() == expectedLen);
    BencTestSerialization(obj, benc);
    delete obj;
}

static void BencTestParseDicts()
{
    BencObj *obj;

    obj = BencObj::Decode("d");
    utassert(!obj);
    obj = BencObj::Decode("d123");
    utassert(!obj);
    obj = BencObj::Decode("di12e");
    utassert(!obj);
    obj = BencObj::Decode("di12e2:ale");
    utassert(!obj);

    BencTestParseDict("de", 0);
    BencTestParseDict("d2:hai35ee", 1);
    BencTestParseDict("d4:borg1:a3:rum3:leee", 2);
    BencTestParseDict("d1:Zi-23e2:able3:keyi35ee", 3);
}

#define ITERATION_COUNT 128

static void BencTestArrayAppend()
{
    BencArray *array = new BencArray();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        array->Add(i);
        utassert(array->Length() == i);
    }
    array->Add(new BencDict());
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        BencInt *obj = array->GetInt(i - 1);
        utassert(obj && obj->Type() == BT_INT);
        utassert(obj->Value() == (int64_t)i);
        utassert(!array->GetString(i - 1));
        utassert(!array->GetArray(i - 1));
        utassert(!array->GetDict(i - 1));
    }
    utassert(!array->GetInt(ITERATION_COUNT));
    utassert(array->GetDict(ITERATION_COUNT));
    BencTestRoundtrip(array);
    delete array->Remove(ITERATION_COUNT);
    delete array->Remove(0);
    delete array->Remove(ITERATION_COUNT + 13);
    utassert(array->Length() == ITERATION_COUNT - 1);
    utassert(array->GetInt(0)->Value() == 2);
    utassert(array->GetInt(ITERATION_COUNT - 2)->Value() == ITERATION_COUNT);
    BencTestRoundtrip(array);
    delete array;
}

static void BencTestDictAppend()
{
    /* test insertion in ascending order */
    BencDict *dict = new BencDict();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        ScopedMem<char> key(str::Format("%04u", i));
        utassert(str::Len(key) == 4);
        dict->Add(key, i);
        utassert(dict->Length() == i);
        utassert(dict->GetInt(key));
        utassert(!dict->GetString(key));
        utassert(!dict->GetArray(key));
        utassert(!dict->GetDict(key));
    }
    BencInt *intObj = dict->GetInt("0123");
    utassert(intObj && intObj->Value() == 123);
    BencTestRoundtrip(dict);
    delete dict;

    /* test insertion in descending order */
    dict = new BencDict();
    for (size_t i = ITERATION_COUNT; i > 0; i--) {
        ScopedMem<char> key(str::Format("%04u", i));
        utassert(str::Len(key) == 4);
        BencObj *obj = new BencInt(i);
        dict->Add(key, obj);
        utassert(dict->Length() == ITERATION_COUNT + 1 - i);
        utassert(dict->GetInt(key));
    }
    intObj = dict->GetInt("0123");
    utassert(intObj && intObj->Value() == 123);
    BencTestRoundtrip(dict);
    delete dict;

    dict = new BencDict();
    dict->Add("ab", 1);
    dict->Add("KL", 2);
    dict->Add("gh", 3);
    dict->Add("YZ", 4);
    dict->Add("ab", 5);
    BencTestSerialization(dict, "d2:KLi2e2:YZi4e2:abi5e2:ghi3ee");
    delete dict->Remove("gh");
    delete dict->Remove("YZ");
    delete dict->Remove("missing");
    BencTestSerialization(dict, "d2:KLi2e2:abi5ee");
    delete dict;
}

static void GenRandStr(char *buf, int bufLen)
{
    int l = rand() % (bufLen - 1);
    for (int i = 0; i < l; i++) {
        char c = (char)(33 + (rand() % (174 - 33)));
        buf[i] = c;
    }
    buf[l] = 0;
}

static void GenRandTStr(WCHAR *buf, int bufLen)
{
    int l = rand() % (bufLen - 1);
    for (int i = 0; i < l; i++) {
        WCHAR c = (WCHAR)(33 + (rand() % (174 - 33)));
        buf[i] = c;
    }
    buf[l] = 0;
}

static void BencTestStress()
{
    char key[64];
    char val[64];
    WCHAR tval[64];
    Vec<BencObj*> stack(29);
    BencDict *startDict = new BencDict();
    BencDict *d = startDict;
    BencArray *a = NULL;
    srand((unsigned int)time(NULL));
    // generate new dict or array with 5% probability each, close an array or
    // dict with 8% probability (less than 10% probability of opening one, to
    // encourage nesting), generate int, string or raw strings uniformly
    // across the remaining 72% probability
    for (int i = 0; i < 10000; i++)
    {
        int n = rand() % 100;
        if (n < 5) {
            BencDict *nd = new BencDict();
            if (a) {
                a->Add(nd);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, nd);
            }
            stack.Push(nd);
            d = nd;
            a = NULL;
        } else if (n < 10) {
            BencArray *na = new BencArray();
            if (a) {
                a->Add(na);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, na);
            }
            stack.Push(na);
            d = NULL;
            a = na;
        } else if (n < 18) {
            if (stack.Count() > 0) {
                n = rand() % 100;
                stack.Pop();
                BencObj *o = startDict;
                if (stack.Count() > 0) {
                    o = stack.Last();
                }
                a = NULL; d = NULL;
                if (BT_ARRAY == o->Type()) {
                    a = static_cast<BencArray *>(o);
                } else {
                    d = static_cast<BencDict *>(o);
                }
            }
        } else if (n < (18 + 24)) {
            int64_t v = rand();
            if (a) {
                a->Add(v);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, v);
            }
        } else if (n < (18 + 24 + 24)) {
            GenRandStr(val, dimof(val));
            if (a) {
                a->AddRaw(val);
            } else {
                GenRandStr(key, dimof(key));
                d->AddRaw(key, val);
            }
        } else {
            GenRandTStr(tval, dimof(tval));
            if (a) {
                a->Add(tval);
            } else {
                GenRandStr(key, dimof(key));
                d->Add(key, tval);
            }
        }
    }

    ScopedMem<char> s(startDict->Encode());
    delete startDict;
}

void BencTest()
{
    BencTestParseInt();
    BencTestParseString();
    BencTestParseRawStrings();
    BencTestParseArrays();
    BencTestParseDicts();
    BencTestArrayAppend();
    BencTestDictAppend();
    BencTestStress();
}
