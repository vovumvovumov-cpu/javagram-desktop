/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"
#include "base/timer.h"

class QLineEdit;

namespace Intro {
namespace details {

class PhoneWidget final : public Step {
public:
	PhoneWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	QString accessibilityName() override;


	void setInnerFocus() override;
	void activate() override;
	void finished() override;
	void cancelled() override;
	void submit() override;

	bool hasBack() const override {
		return true;
	}

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void setupQrLogin();
	void checkRequest();

	void phoneSubmitDone(const MTPauth_SentCode &result);
	void phoneSubmitFail(const MTP::Error &error);

	QString fullNumber() const;
	void stopCheck();

	void showPhoneError(rpl::producer<QString> text);
	void hidePhoneError();

	QLineEdit *_phone = nullptr;

	QString _sentPhone;
	mtpRequestId _sentRequest = 0;

	base::Timer _checkRequestTimer;

};

} // namespace details
} // namespace Intro
