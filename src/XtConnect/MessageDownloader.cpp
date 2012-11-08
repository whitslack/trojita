/*
    Certain enhancements (www.xtuple.com/trojita-enhancements)
    are copyright © 2010 by OpenMFG LLC, dba xTuple.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    - Neither the name of xTuple nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior
    written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "MessageDownloader.h"
#include "Imap/Model/FindInterestingPart.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"

//#define DEBUG_PENDING_MESSAGES
//#define DEBUG_PENDING_MESSAGES_2

namespace XtConnect {

MessageDownloader::MessageDownloader(QObject *parent, const QString &mailboxName ):
    QObject(parent), lastModel(0), registeredMailbox(mailboxName)
{
    m_releasingTimer = new QTimer(this);
    m_releasingTimer->setSingleShot(true);
    connect(m_releasingTimer, SIGNAL(timeout()), this, SLOT(slotFreeProcessedMessages()));
}

void MessageDownloader::requestDownload( const QModelIndex &message )
{
    if ( ! lastModel ) {
        lastModel = message.model();
        connect( lastModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotDataChanged(QModelIndex,QModelIndex)) );
    }
    Q_ASSERT(lastModel == message.model());

    Q_ASSERT( message.parent().parent().data( Imap::Mailbox::RoleMailboxName ).toString() == registeredMailbox );

    MessageMetadata metaData;

    // Now request loading of the message metadata. We are especially interested in the message envelope, the part which will
    // enable us to request the main part in future.
    metaData.hasMessage = message.data( Imap::Mailbox::RoleMessageMessageId ).isValid();

    const uint uid = message.data( Imap::Mailbox::RoleMessageUid ).toUInt();
    Q_ASSERT(uid);
    m_parts[ uid ] = metaData;

    if (metaData.hasMessage) {
        requestDataDownload(message);
    }
}

/** @short Request a download of the actual bulk data for the given message */
void MessageDownloader::requestDataDownload(const QModelIndex &message)
{
    const uint uid = message.data( Imap::Mailbox::RoleMessageUid ).toUInt();
    Q_ASSERT(uid);

    QMap<uint,MessageMetadata>::iterator it = m_parts.find(uid);
    Q_ASSERT(it != m_parts.end());
    Q_ASSERT(it->hasMessage);

    // Let's see if we can find out what the "main part" is.
    // The "main part" cannot be determined prior to the message's metadata becoming available.

    QModelIndex mainPart;
    Imap::Mailbox::FindInterestingPart::MainPartReturnCode status =
            Imap::Mailbox::FindInterestingPart::findMainPartOfMessage(message, mainPart, it->partMessage, 0);
    it->mainPart = mainPart;
    switch (status) {
    case Imap::Mailbox::FindInterestingPart::MAINPART_FOUND:
    case Imap::Mailbox::FindInterestingPart::MAINPART_PART_LOADING:
        // The MAINPART_PART_LOADING is a confusing name -- as we're calling findMainPartOfMessage with the last parameter being
        // nullptr, the function will not attempt to actually fetch the data, and therefore the exit status cannot be
        // MAINPART_FOUND, so it will be MAINPART_PART_LOADING. Yep, confusing.
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Requesting data for " << uid;
#endif
        // Ask for the data.
        Q_ASSERT(mainPart.isValid());
        mainPart.data(Imap::Mailbox::RolePartData);
        break;

    case Imap::Mailbox::FindInterestingPart::MAINPART_MESSAGE_NOT_LOADED:
        Q_ASSERT(false);
        break;

    case Imap::Mailbox::FindInterestingPart::MAINPART_PART_CANNOT_DETERMINE:
        it->hasMainPart = true;
        it->mainPartFailed = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Cannot find the main part for " << uid;
#endif
        break;
    }

    // Now request the rest of the data
    const QAbstractItemModel *model = message.model();
    Q_ASSERT(model);
    QModelIndex header = model->index(0, Imap::Mailbox::TreeItem::OFFSET_HEADER, message);
    header.data(Imap::Mailbox::RolePartData);
    it->hasHeader = header.data(Imap::Mailbox::RoleIsFetched).toBool();
    QModelIndex text = model->index(0, Imap::Mailbox::TreeItem::OFFSET_TEXT, message);
    text.data(Imap::Mailbox::RolePartData);
    it->hasBody = text.data(Imap::Mailbox::RoleIsFetched).toBool();

    if (it->hasMainPart && it->hasHeader && it->hasBody) {
        // We have everything what we need at this point
        slotDataChanged(message, message);
    }
}

void MessageDownloader::slotDataChanged( const QModelIndex &a, const QModelIndex &b )
{
    if ( ! a.isValid() ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "MessageDownloader::slotDataChanged: a not valid" << a;
#endif
        return;
    }

    if ( a != b ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "MessageDownloader::slotDataChanged: a != b" << a;
#endif
        return;
    }

    QModelIndex message = Imap::Mailbox::Model::findMessageForItem( a );
    if ( ! message.isValid() ) {
#ifdef DEBUG_PENDING_MESSAGES_2
        qDebug() << "MessageDownloader::slotDataChanged: message not valid" << a;
#endif
        return;
    }

    if ( message.parent().parent().data( Imap::Mailbox::RoleMailboxName ).toString() != registeredMailbox ) {
#ifdef DEBUG_PENDING_MESSAGES_2
        qDebug() << "MessageDownloader::slotDataChanged: not this mailbox" << a <<
                    message.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString() << registeredMailbox;
#endif
        return;
    }

    const uint uid = message.data( Imap::Mailbox::RoleMessageUid ).toUInt();
    Q_ASSERT(uid);

    QMap<uint,MessageMetadata>::iterator it = m_parts.find( uid );
    if ( it == m_parts.end() ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "We are not interested in message with UID" << uid;
#endif
        return;
    }

    const QAbstractItemModel *model = message.model();

    // Find out whether the data is available already
    QModelIndex header = model->index(0, Imap::Mailbox::TreeItem::OFFSET_HEADER, message);

    if (!it->hasHeader && header.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        it->hasHeader = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got header for" << uid;
#endif
    }

    QModelIndex text = model->index(0, Imap::Mailbox::TreeItem::OFFSET_TEXT, message);
    if (!it->hasBody && text.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        it->hasBody = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got body for" << uid;
#endif
    }

    if (!it->hasMainPart && !it->mainPartFailed && it->mainPart.isValid() && a == it->mainPart &&
            it->mainPart.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        it->hasMainPart = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got main part for" << uid;
#endif
    }

    if (a == message && !it->hasMessage) {

        if (!message.data(Imap::Mailbox::RoleIsFetched).toBool()) {
#ifdef DEBUG_PENDING_MESSAGES_2
            qDebug() << "  Not loaded yet";
#endif
            return;
        }

        it->hasMessage = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got message for" << uid;
#endif
        requestDataDownload(message);

    }

    if ( it->hasHeader && it->hasBody && it->hasMessage && it->hasMainPart ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "We've got everything for" << uid << "-> saving";
#endif
        // Check message metadata
        Q_ASSERT(message.data(Imap::Mailbox::RoleMessageMessageId).isValid());
        Q_ASSERT(message.data(Imap::Mailbox::RoleMessageSubject).isValid());
        Q_ASSERT(message.data(Imap::Mailbox::RoleMessageDate).isValid());

        // Check the main part
        QVariant mainPartData = it->mainPart.data(Imap::Mailbox::RolePartData);
        QString mainPart;
        if (it->mainPartFailed) {
            mainPart = it->partMessage;
        } else {
            Q_ASSERT(mainPartData.isValid());
            mainPart = mainPartData.toString();
        }

        // The rest of the bulk data
        QVariant headerData = header.data(Imap::Mailbox::RolePartData);
        Q_ASSERT(headerData.isValid());
        QVariant bodyData = text.data(Imap::Mailbox::RolePartData);
        Q_ASSERT(bodyData.isValid());

//#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Stored" << registeredMailbox << uid;
//#endif
        emit messageDownloaded( message, headerData.toByteArray(), bodyData.toByteArray(), mainPart );
        m_parts.erase(it);

        m_messagesToBeFreed << message;
        if (!m_releasingTimer->isActive())
            m_releasingTimer->start();
    } else {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Something is missing for" << uid << it->hasHeader << it->hasBody << it->hasMessage << it->hasMainPart;
#endif
    }
}

int MessageDownloader::pendingMessages() const
{
    return m_parts.size();
}

/** @short Instruct the Model that the data it has cached for a particular message is no longer needed */
void MessageDownloader::slotFreeProcessedMessages()
{
    Q_FOREACH(QPersistentModelIndex index, m_messagesToBeFreed) {
        if (!index.isValid())
            continue;
        // The const_cast should be safe here -- this action is certainly not going to invalidate the index,
        // and even the releaseMessageData() won't (directly) touch its members anyway...
        Imap::Mailbox::Model *model = qobject_cast<Imap::Mailbox::Model*>(const_cast<QAbstractItemModel*>(index.model()));
        Q_ASSERT(model);
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Freeing memory for" << index.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString() <<
                    "UID" << index.data(Imap::Mailbox::RoleMessageUid).toUInt();
#endif
        model->releaseMessageData(index);
    }
    m_messagesToBeFreed.clear();
}

}
