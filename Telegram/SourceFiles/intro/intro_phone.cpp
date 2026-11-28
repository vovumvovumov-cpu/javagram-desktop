/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_phone.h"

#include "lang/lang_keys.h"
#include "intro/intro_code.h"
#include "intro/intro_email.h"
#include "intro/intro_qr.h"
#include "styles/style_intro.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/widgets/fields/special_fields.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "ui/boxes/confirm_box.h"
#include "boxes/abstract_box.h"
#include "boxes/phone_banned_box.h"
#include "core/application.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include <QtWidgets/QLineEdit>

namespace Intro {
namespace details {
namespace {

[[nodiscard]] QString EncodeZentrogramId(const QString &value) {
	const auto id = value.trimmed().toUpper();
	static const auto Pattern = QRegularExpression("^JG-[0-9A-F]{10}$");
	if (!Pattern.match(id).hasMatch()) {
		return QString();
	}
	const auto email = QString("jg-%1@zentrogram.invalid").arg(id.mid(3).toLower());
	QString encoded = "888";
	for (const auto ch : email) {
		switch (ch.unicode()) {
		case '@': encoded += "q0"; break;
		case '.': encoded += "q1"; break;
		case '-': encoded += "q2"; break;
		case '_': encoded += "q3"; break;
		case '+': encoded += "q4"; break;
		case 'q': encoded += "q5"; break;
		default: encoded += ch;
		}
	}
	return encoded;
}

} // namespace

PhoneWidget::PhoneWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data)
, _phone(new QLineEdit(this))
, _checkRequestTimer([=] { checkRequest(); }) {
	_phone->setAccessibleName("Zentrogram virtual ID");
	_phone->setPlaceholderText("JG-0123ABCDEF");
	_phone->setMaxLength(13);
	_phone->setMinimumHeight(st::introPhone.height);
	_phone->setClearButtonEnabled(true);
	_phone->setInputMethodHints(Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
	connect(_phone, &QLineEdit::textChanged, [=] { hidePhoneError(); });
	setTitleText(tr::lng_phone_title());
	setDescriptionText(tr::lng_phone_desc());
	setErrorCentered(true);
	setupQrLogin();
}

QString PhoneWidget::accessibilityName() {
	return tr::lng_phone_title(tr::now);
}

void PhoneWidget::setupQrLogin() {
	const auto qrLogin = Ui::CreateChild<Ui::LinkButton>(
		this,
		tr::lng_phone_to_qr(tr::now));
	qrLogin->show();

	DEBUG_LOG(("PhoneWidget.qrLogin link created and shown."));

	rpl::combine(
		sizeValue(),
		qrLogin->widthValue()
	) | rpl::on_next([=](QSize size, int qrLoginWidth) {
		qrLogin->moveToLeft(
			(size.width() - qrLoginWidth) / 2,
			contentTop() + st::introQrLoginLinkTop);
	}, qrLogin->lifetime());

	qrLogin->setClickedCallback([=] {
		goNextOrBack<QrWidget>();
	});
}

void PhoneWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	_phone->setGeometry(contentLeft(), contentTop() + st::introStepFieldTop, st::introPhone.width, st::introPhone.height);
}

void PhoneWidget::showPhoneError(rpl::producer<QString> text) {
	_phone->setStyleSheet("border: 1px solid #d9534f;");
	showError(std::move(text));
}

void PhoneWidget::hidePhoneError() {
	hideError();
}

void PhoneWidget::submit() {
	if (_sentRequest || isHidden()) {
		return;
	}
	const auto visibleId = _phone->text().trimmed().toUpper();
	const auto phone = EncodeZentrogramId(visibleId);
	if (phone.isEmpty()) {
		showPhoneError(rpl::single("Enter a valid Zentrogram ID: JG- followed by 10 hex characters."));
		_phone->setFocus();
		return;
	}
	cancelNearestDcRequest();
	hidePhoneError();
	_checkRequestTimer.callEach(1000);
	getData()->virtualId = visibleId;
	_sentPhone = phone;
	api().instance().setUserPhone(_sentPhone);
	_sentRequest = api().request(MTPauth_SendCode(
		MTP_string(_sentPhone),
		MTP_int(ApiId),
		MTP_string(ApiHash),
		MTP_codeSettings(
			MTP_flags(0),
			MTPVector<MTPbytes>(),
			MTPstring(),
			MTPBool())
	)).done([=](const MTPauth_SentCode &result) {
		phoneSubmitDone(result);
	}).fail([=](const MTP::Error &error) {
		phoneSubmitFail(error);
	}).handleFloodErrors().send();
}

void PhoneWidget::stopCheck() {
	_checkRequestTimer.cancel();
}

void PhoneWidget::checkRequest() {
	auto status = api().instance().state(_sentRequest);
	if (status < 0) {
		auto leftms = -status;
		if (leftms >= 1000) {
			api().request(base::take(_sentRequest)).cancel();
		}
	}
	if (!_sentRequest && status == MTP::RequestSent) {
		stopCheck();
	}
}

void PhoneWidget::phoneSubmitDone(const MTPauth_SentCode &result) {
	stopCheck();
	_sentRequest = 0;

	result.match([&](const MTPDauth_sentCode &data) {
		fillSentCodeData(data);
		getData()->phone = _sentPhone;
		getData()->phoneHash = qba(data.vphone_code_hash());
		if (getData()->emailStatus == EmailStatus::SetupRequired) {
			return goNext<EmailWidget>();
		}
		const auto next = data.vnext_type();
		if (next && next->type() == mtpc_auth_codeTypeCall) {
			getData()->callStatus = CallStatus::Waiting;
			getData()->callTimeout = data.vtimeout().value_or(60);
		} else {
			getData()->callStatus = CallStatus::Disabled;
			getData()->callTimeout = 0;
		}
		goNext<CodeWidget>();
	}, [&](const MTPDauth_sentCodeSuccess &data) {
		finish(data.vauthorization());
	}, [](const MTPDauth_sentCodePaymentRequired &) {
		LOG(("API Error: Unexpected auth.sentCodePaymentRequired "
			"(PhoneWidget::phoneSubmitDone)."));
	});
}

void PhoneWidget::phoneSubmitFail(const MTP::Error &error) {
	if (MTP::IsFloodError(error)) {
		stopCheck();
		_sentRequest = 0;
		showPhoneError(tr::lng_flood_error());
		return;
	}

	stopCheck();
	_sentRequest = 0;
	const auto &err = error.type();
	if (err == u"PHONE_NUMBER_FLOOD"_q) {
		Ui::show(Ui::MakeInformBox(tr::lng_error_phone_flood()));
	} else if (err == u"PHONE_NUMBER_INVALID"_q) { // show error
		showPhoneError(rpl::single("Zentrogram server did not accept this virtual ID."));
	} else if (err == u"PHONE_NUMBER_BANNED"_q) {
		Ui::ShowPhoneBannedError(getData()->controller, _sentPhone);
	} else if (!MTP::IgnoreError(error)) {
		showPhoneError(rpl::single(err));
	}
}

QString PhoneWidget::fullNumber() const {
	return _phone->text().trimmed();
}

void PhoneWidget::setInnerFocus() {
	_phone->setFocus();
}

void PhoneWidget::activate() {
	Step::activate();
	showChildren();
	setInnerFocus();
}

void PhoneWidget::finished() {
	Step::finished();
	_checkRequestTimer.cancel();
	apiClear();

	cancelled();
}

void PhoneWidget::cancelled() {
	api().request(base::take(_sentRequest)).cancel();
}

} // namespace details
} // namespace Intro
