# Zentrogram Numbers bot

Initial Telegram bot implementation for `@Zentrogram_NumberBot`.

## Included now

- `/start` welcome message and reply-keyboard menu
- `/balance` and `⭐ Мой баланс`
- `/mynumbers` and `📞 Мои номера`
- `/getnumber` informational response
- SQLite user records and a restricted admin-only `/grant` command for demo Z-Stars

This is a starter bot, not a live phone-number rental service. It does not issue numbers,
receive or forward one-time passwords, or provide numbers for Telegram account
registration or bypassing Telegram's safeguards. A future SMS integration must be
for a service we operate, use a legitimate provider, and comply with applicable law.

## Run locally

1. Create a private deployment environment.
2. Set `TELEGRAM_BOT_TOKEN` to the current token from @BotFather. Never commit it.
3. Optionally set `BOT_ADMIN_IDS` to comma-separated numeric Telegram user IDs.
4. Run:

```sh
cd zentrogram-bot
python -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python bot.py
```

For persistent hosting, deploy this folder as a Docker worker/service and mount
persistent storage for `zentrogram-bot.sqlite3`. Configure the token as a secret
environment variable, not in source code or logs.

## Important

If a bot token has ever been pasted into a chat or committed to source control,
revoke it in @BotFather and use a fresh token only as a deployment secret.
