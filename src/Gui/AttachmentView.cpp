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
#include "AttachmentView.h"
#include "Imap/Network/FileDownloadManager.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ItemRoles.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>

namespace Gui
{

AttachmentView::AttachmentView(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex):
    QWidget(parent), fileDownloadManager(0)
{
    fileDownloadManager = new Imap::Network::FileDownloadManager(this, manager, partIndex);
    QHBoxLayout *layout = new QHBoxLayout(this);
    QLabel *lbl = new QLabel(tr("Attachment %1 (%2)").arg(partIndex.data(Imap::Mailbox::RolePartFileName).toString(),
                             partIndex.data(Imap::Mailbox::RolePartMimeType).toString()));
    layout->addWidget(lbl);
    QPushButton *downloadButton = new QPushButton(tr("Download"));
    downloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(downloadButton);
    connect(downloadButton, SIGNAL(clicked()), fileDownloadManager, SLOT(slotDownloadNow()));
    connect(fileDownloadManager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void AttachmentView::slotFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Attachment"), *fileName, QString(), 0, QFileDialog::HideNameFilterDetails);
}

void AttachmentView::slotTransferError(const QString &errorString)
{
    QMessageBox::critical(this, tr("Can't save attachment"), tr("Unable to save the attachment. Error:\n%1").arg(errorString));
}

}
