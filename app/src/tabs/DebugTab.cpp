#include "DebugTab.h"

#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "ngpc/core.h"

DebugTab::DebugTab(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    info_ = new QPlainTextEdit(this);
    info_->setReadOnly(true);

    info_->appendPlainText("NGPC Sound Creator - Debug");
    info_->appendPlainText(QString("Core version: %1.%2.%3")
                               .arg(ngpc::Version::kMajor)
                               .arg(ngpc::Version::kMinor)
                               .arg(ngpc::Version::kPatch));
    info_->appendPlainText("MVP mode: K1Sound (SNK-like)");

    root->addWidget(info_, 1);
}
