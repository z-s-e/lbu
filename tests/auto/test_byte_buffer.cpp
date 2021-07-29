/* Copyright 2015-2017 Zeno Sebastian Endemann <zeno.endemann@mailbox.org>
 *
 * This file is part of the lbu library.
 *
 * lbu is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the licence, or (at your option) any later version.
 *
 * lbu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <QString>
#include <QtTest>

#include <lbu/byte_buffer.h>

using namespace lbu;

class Test_byte_buffer : public QObject
{
    Q_OBJECT

public:
    Test_byte_buffer() = default;

private Q_SLOTS:
    void testConstructor();
    void testReserve();
    void testModify();
};

void Test_byte_buffer::testConstructor()
{
    {
        byte_buffer b;

        QVERIFY(b.is_empty());
        QCOMPARE(b.capacity(), size_t(15));

        QVERIFY(b.data());
        QCOMPARE(*static_cast<char*>(b.data()), char(0));
    }

    {
        const char tmp[] = "a";
        byte_buffer b(array_ref<const char>(tmp, 1));

        QCOMPARE(b.size(), size_t(1));
        QCOMPARE(b.capacity(), size_t(15));

        QVERIFY(b.data());
        QCOMPARE(std::memcmp(b.data(), tmp, 1), 0);
    }

    {
        const char tmp[] = "123456789012345";
        byte_buffer b(array_ref<const char>(tmp, 15));
        byte_buffer b2(b);

        QCOMPARE(b.size(), size_t(15));
        QCOMPARE(b.capacity(), size_t(15));

        QVERIFY(b.data());
        QCOMPARE(std::memcmp(b.data(), tmp, 15), 0);

        QCOMPARE(b2.size(), size_t(15));
        QCOMPARE(b2.capacity(), size_t(15));

        QVERIFY(b2.data());
        QCOMPARE(std::memcmp(b2.data(), tmp, 15), 0);
    }

    {
        const char tmp[] = "1234567890123456";
        byte_buffer b(array_ref<const char>(tmp, 16));

        QCOMPARE(b.size(), size_t(16));
        QVERIFY(b.capacity() > size_t(15));

        QVERIFY(b.data());
        QCOMPARE(std::memcmp(b.data(), tmp, 16), 0);

        byte_buffer b2(std::move(b));
        QCOMPARE(b2.size(), size_t(16));
        QVERIFY(b2.capacity() > size_t(15));

        QVERIFY(b2.data());
        QCOMPARE(std::memcmp(b2.data(), tmp, 16), 0);
    }
}

void Test_byte_buffer::testReserve()
{
    byte_buffer b;

    for( unsigned i = 0; i <= 15; ++i ) {
        b.reserve(i);
        QCOMPARE(b.capacity(), size_t(15));
        QVERIFY(b.is_empty());
    }

    b.reserve(16);
    QCOMPARE(b.capacity(), size_t(16));
    QVERIFY(b.is_empty());

    b.shrink_to_fit();
    QCOMPARE(b.capacity(), size_t(15));
    QVERIFY(b.is_empty());

    b.set_capacity(1);
    QCOMPARE(b.capacity(), size_t(15));
    QVERIFY(b.is_empty());

    b.reserve(123);
    QCOMPARE(b.capacity(), size_t(123));
    b.reserve(20);
    QCOMPARE(b.capacity(), size_t(123));
    b.set_capacity(20);
    QCOMPARE(b.capacity(), size_t(20));
    b.auto_grow_reserve();
    QVERIFY(b.capacity() > size_t(20));
    QVERIFY(b.is_empty());
}

void Test_byte_buffer::testModify()
{
    byte_buffer b;

    b.append(3, 'x');
    QCOMPARE(b.size(), size_t(3));
    QCOMPARE(std::memcmp(b.data(), "xxx", 3), 0);

    b.append(5, 'y');
    QCOMPARE(b.size(), size_t(8));
    QCOMPARE(std::memcmp(b.data(), "xxxyyyyy", 8), 0);

    auto tmp = b.append_begin();
    QCOMPARE(tmp.size(), size_t(7));
    std::memcpy(tmp.data(), "zz", 2);
    b.append_commit(2);

    QCOMPARE(b.size(), size_t(10));
    QCOMPARE(std::memcmp(b.data(), "xxxyyyyyzz", 10), 0);

    b.append(5, 'a');
    QCOMPARE(b.size(), size_t(15));
    QCOMPARE(std::memcmp(b.data(), "xxxyyyyyzzaaaaa", 15), 0);

    b.append(1, 'b');
    QCOMPARE(b.size(), size_t(16));
    QCOMPARE(std::memcmp(b.data(), "xxxyyyyyzzaaaaab", 16), 0);

    b.append(50, 'q');
    QCOMPARE(b.size(), size_t(66));

    b.chop(50);
    QCOMPARE(b.size(), size_t(16));
    QCOMPARE(std::memcmp(b.data(), "xxxyyyyyzzaaaaab", 16), 0);

    b.erase(15, 1);
    QCOMPARE(b.size(), size_t(15));
    QCOMPARE(std::memcmp(b.data(), "xxxyyyyyzzaaaaa", 15), 0);

    b.erase(0, 3);
    QCOMPARE(b.size(), size_t(12));
    QCOMPARE(std::memcmp(b.data(), "yyyyyzzaaaaa", 12), 0);

    b.erase(5, 2);
    QCOMPARE(b.size(), size_t(10));
    QCOMPARE(std::memcmp(b.data(), "yyyyyaaaaa", 10), 0);

    b.insert(0, 3, 'c');
    QCOMPARE(b.size(), size_t(13));
    QCOMPARE(std::memcmp(b.data(), "cccyyyyyaaaaa", 13), 0);

    b.insert(13, 2, 'd');
    QCOMPARE(b.size(), size_t(15));
    QCOMPARE(std::memcmp(b.data(), "cccyyyyyaaaaadd", 15), 0);

    b.insert(8, 5, 'e');
    QCOMPARE(b.size(), size_t(20));
    QCOMPARE(std::memcmp(b.data(), "cccyyyyyeeeeeaaaaadd", 20), 0);

    b.insert(8, 100, 'w');
    QCOMPARE(b.size(), size_t(120));

    b.resize(8);
    QCOMPARE(b.size(), size_t(8));
    QCOMPARE(std::memcmp(b.data(), "cccyyyyy", 8), 0);

    b.clear();
    QVERIFY(b.is_empty());
}

QTEST_APPLESS_MAIN(Test_byte_buffer)

#include "test_byte_buffer.moc"
