/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QtTest>
#include "test_Imap_Threading.h"
#include "../headless_test.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Streams/FakeSocket.h"
#include "test_LibMailboxSync/FakeCapabilitiesInjector.h"

/** @short */
void ImapModelThreadingTest::testStaticThreading()
{
    QCOMPARE(SOCK->writtenStuff(), t.mk("UID THREAD REFS utf-8 ALL\r\n"));
    SOCK->fakeReading(QByteArray("* THREAD (1)(2)(3)(4)(5)(6)(7)(8)(9)(10)\r\n") + t.last("OK thread\r\n"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QMap<QString, int> m;
    m["0"] = 1; // index 0: UID 1
    m["0.0"] = 0; // index 0.0: invalid
    m["0.1"] = 0; // index 0.1: invalid
    m["1"] = 2;
    m["2"] = 3;
    m["3"] = 4;
    m["4"] = 5;
    m["5"] = 6;
    m["6"] = 7;
    m["7"] = 8;
    m["8"] = 9;
    m["9"] = 10; // index 9: UID 10
    m["10"] = 0; // index 10: invalid
    verify(m);
    QVERIFY(SOCK->writtenStuff().isEmpty());
    QVERIFY(errorSpy->isEmpty());
}

QModelIndex ImapModelThreadingTest::findItem(const QList<int> &where)
{
    QModelIndex index = msgListA;
    for (QList<int>::const_iterator it = where.begin(); it != where.end(); ++it) {
        index = index.child(*it, 0);
        if (it + 1 != where.end()) {
            // this index is an intermediate one in the path, hence it should not really fail
            Q_ASSERT(index.isValid());
        }
    }
    return index;
}

QModelIndex ImapModelThreadingTest::findItem(const QString &where)
{
    QStringList list = where.split(QLatin1Char('.'));
    Q_ASSERT(!list.isEmpty());
    QList<int> items;
    Q_FOREACH(const QString number, list) {
        bool ok;
        items << number.toInt(&ok);
        Q_ASSERT(ok);
    }
    return findItem(items);
}

void ImapModelThreadingTest::verify(const QMap<QString, int> &mapping)
{
    for(QMap<QString, int>::const_iterator it = mapping.begin(); it != mapping.end(); ++it) {
        QModelIndex index = findItem(it.key());
        if (it.value()) {
            // it's a supposedly valid index
            QVERIFY(index.isValid());
            QCOMPARE(index.data(Imap::Mailbox::RoleMessageUid).toInt(), it.value());
        } else {
            QVERIFY(!index.isValid());
        }
    }
}

void ImapModelThreadingTest::initTestCase()
{
    LibMailboxSync::initTestCase();
    msgListModel = 0;
    threadingModel = 0;
}

void ImapModelThreadingTest::init()
{
    LibMailboxSync::init();

    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("THREAD=REFS"));

    existsA = 10;
    uidValidityA = 333;
    for (uint i = 1; i <= existsA; ++i) {
        uidMapA << i;
    }
    uidNextA = 6;
    helperSyncAWithMessagesEmptyState();

    msgListModel = new Imap::Mailbox::MsgListModel(this, model);
    msgListModel->setMailbox(idxA);
    threadingModel = new Imap::Mailbox::ThreadingMsgListModel(this);
    threadingModel->setSourceModel(msgListModel);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

void ImapModelThreadingTest::cleanup()
{
    LibMailboxSync::cleanup();
    threadingModel->deleteLater();
    threadingModel = 0;
    msgListModel->deleteLater();
    msgListModel = 0;
}

TROJITA_HEADLESS_TEST( ImapModelThreadingTest )