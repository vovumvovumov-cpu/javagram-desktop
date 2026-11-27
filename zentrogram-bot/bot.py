import logging
import os
import sqlite3
from pathlib import Path

from telegram import ReplyKeyboardMarkup, Update
from telegram.ext import Application, CommandHandler, ContextTypes, MessageHandler, filters

logging.basicConfig(
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    level=logging.INFO,
)
log = logging.getLogger("zentrogram-bot")

DB_PATH = Path(os.getenv("BOT_DB_PATH", "zentrogram-bot.sqlite3"))
TOKEN = os.environ["TELEGRAM_BOT_TOKEN"]
ADMIN_IDS = {
    int(value.strip())
    for value in os.getenv("BOT_ADMIN_IDS", "").split(",")
    if value.strip().isdigit()
}

KEYBOARD = ReplyKeyboardMarkup(
    [
        ["🇷🇺 Получить номер", "⭐ Мой баланс"],
        ["📞 Мои номера", "ℹ️ Помощь"],
    ],
    resize_keyboard=True,
)


def db():
    connection = sqlite3.connect(DB_PATH)
    connection.execute(
        """CREATE TABLE IF NOT EXISTS users (
            telegram_id INTEGER PRIMARY KEY,
            username TEXT,
            stars INTEGER NOT NULL DEFAULT 0
        )"""
    )
    connection.commit()
    return connection


def ensure_user(user):
    with db() as connection:
        connection.execute(
            """INSERT INTO users (telegram_id, username)
               VALUES (?, ?)
               ON CONFLICT(telegram_id) DO UPDATE SET username=excluded.username""",
            (user.id, user.username or ""),
        )


def get_balance(user_id):
    with db() as connection:
        row = connection.execute(
            "SELECT stars FROM users WHERE telegram_id=?", (user_id,)
        ).fetchone()
    return row[0] if row else 0


async def start(update: Update, context: ContextTypes.DEFAULT_TYPE):
    ensure_user(update.effective_user)
    await update.message.reply_text(
        "Добро пожаловать в Zentrogram Numbers! 🇷🇺⭐\n\n"
        "Здесь будет личный баланс Z-Stars и информация о доступных сервисах. "
        "Выдача телефонных номеров пока не подключена.",
        reply_markup=KEYBOARD,
    )


async def balance(update: Update, context: ContextTypes.DEFAULT_TYPE):
    ensure_user(update.effective_user)
    await update.message.reply_text(
        f"⭐ Твой баланс: {get_balance(update.effective_user.id)} Z-Stars"
    )


async def mynumbers(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "У тебя пока нет выданных номеров. Раздел станет доступен после "
        "подключения законного SMS-провайдера для подтверждения в собственных сервисах."
    )


async def getnumber(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "Выдача телефонных номеров пока отключена. Мы не предоставляем номера "
        "для обхода правил регистрации или подтверждения аккаунтов Telegram. "
        "Позже здесь можно будет подключить SMS-подтверждение для собственных сервисов."
    )


async def help_command(update: Update, context: ContextTypes.DEFAULT_TYPE):
    await update.message.reply_text(
        "/start — открыть меню\n"
        "/getnumber — информация о доступности номеров\n"
        "/balance — баланс Z-Stars\n"
        "/mynumbers — мои номера\n"
        "/help — помощь"
    )


async def grant(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if update.effective_user.id not in ADMIN_IDS:
        await update.message.reply_text("Команда доступна только администратору.")
        return
    if len(context.args) != 2 or not all(arg.isdigit() for arg in context.args):
        await update.message.reply_text("Формат: /grant TELEGRAM_USER_ID КОЛИЧЕСТВО")
        return
    user_id, amount = map(int, context.args)
    if amount < 1 or amount > 100000:
        await update.message.reply_text("Количество должно быть от 1 до 100000.")
        return
    with db() as connection:
        connection.execute(
            """INSERT INTO users (telegram_id, stars) VALUES (?, ?)
               ON CONFLICT(telegram_id) DO UPDATE SET stars=stars+excluded.stars""",
            (user_id, amount),
        )
    await update.message.reply_text(f"Начислено {amount} Z-Stars пользователю {user_id}.")


async def text_router(update: Update, context: ContextTypes.DEFAULT_TYPE):
    text = update.message.text
    if text == "⭐ Мой баланс":
        await balance(update, context)
    elif text == "📞 Мои номера":
        await mynumbers(update, context)
    elif text == "🇷🇺 Получить номер":
        await getnumber(update, context)
    elif text == "ℹ️ Помощь":
        await help_command(update, context)
    else:
        await update.message.reply_text("Выбери действие в меню или отправь /help.")


def main():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    application = Application.builder().token(TOKEN).build()
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("getnumber", getnumber))
    application.add_handler(CommandHandler("balance", balance))
    application.add_handler(CommandHandler("mynumbers", mynumbers))
    application.add_handler(CommandHandler("help", help_command))
    application.add_handler(CommandHandler("grant", grant))
    application.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, text_router))
    log.info("Starting Zentrogram Numbers bot")
    application.run_polling(allowed_updates=Update.ALL_TYPES)


if __name__ == "__main__":
    main()
