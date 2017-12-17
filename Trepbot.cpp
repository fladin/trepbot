#include "Trepbot.h"

#include <iostream>

Trepbot::handlersType Trepbot::handlers = {
    {"message", &Trepbot::MessageHandler},
    {"inline_query", &Trepbot::QueryHandler}};

Trepbot::Trepbot(std::string const& telegramToken,
                 std::string const& yandexToken,
                 Httper& http,
                 Escaper& esc)
    : offset(0),
      http(http),
      esc(esc),
      tUrl(telegramToken, esc),
      yaUrl(yandexToken, esc) {}

void Trepbot::GetInfo() {
  Httper::ContainerType data = http.Get(tUrl.GetMe());

  data.push_back(char(0));
  nlohmann::json answer = nlohmann::json::parse(data.data());

  ownId = answer["result"]["id"].get<size_t>();
  name = answer["result"]["username"].get<std::string>();
}

void Trepbot::ProcessUpdates() {
  // Получаем данные из Телеграма
  http.SetTimeout(60500);
  std::cout << tUrl.GetUpdates(60, offset) << std::endl;
  Httper::ContainerType data = http.Get(tUrl.GetUpdates(60, offset));

  data.push_back(char(0));
  nlohmann::json answer = nlohmann::json::parse(data.data());

  std::cout << "Message: " << std::endl << data.data() << std::endl;

  // Обрабатывем их
  if (false == answer["ok"].get<bool>()) {
    std::cout << "Error" << std::endl;
    return;
  }

  nlohmann::json result = answer["result"];
  size_t max(0);
  for (auto& update : result) {  // перебор элементов секции Update
    for (auto obj = update.begin(); obj != update.end(); ++obj) {
      if (obj.key() == "update_id") {  // определение максимального элемента
        long int id = obj->get<long int>();
        if (id > max) {
          max = id;
        }
      } else if (obj->is_object()) {  // получение имени объекта
        auto it = handlers.find(obj.key());

        if (handlers.end() != it) {
          (this->*(it->second))(*obj);  // вызов хендлера
        }
      }
    }
  }

  offset = max + 1;
}

void Trepbot::MessageHandler(nlohmann::json& msg) {
  static const std::string botname("@trepbot");

  if (msg["text"].empty() || msg["chat"].empty()) {
    return;
  }

  std::string speach = msg["text"].get<std::string>();
  size_t chatId = msg["chat"]["id"].get<size_t>();
  std::cout << "Message: " << std::endl
            << "chat: " << chatId << std::endl
            << "text: " << speach << std::endl;

  bool isNeedAnswer(false), isCommand(false);
  for (auto const& entity : msg["entities"]) {
    std::string type = entity["type"].get<std::string>();
    if ("bot_command" == type) {
      isCommand = true;
    } else if ("mention" == type) {
      size_t offset = entity["offset"].get<size_t>();
      size_t length = entity["length"].get<size_t>();

      if (botname == speach.substr(offset, length) && offset == 0) {
        isNeedAnswer = true;
        speach.erase(offset, length);
      }
    }
  }

  if (!isNeedAnswer || isCommand) {
    return;
  }

  // Все данные есть, можно сгенерировать звук и отправить его
  Httper::ContainerType data = std::move(http.Get(yaUrl.Make(speach)));
  Httper::ContainerType result =
      std::move(http.Post(tUrl.SendVoice(chatId), "voice", "voice", data));
  // http.Get(tUrl.SendMessage(chatId, speach));

  result.push_back(0);
  std::cout << "POST result:" << std::endl << result.data() << std::endl;
}

void Trepbot::QueryHandler(nlohmann::json& query) {
  // Получение идентификатора запроса
  std::string queryId = query["id"].get<std::string>();
  size_t senderId = query["from"]["id"].get<size_t>();
  std::string speach = query["query"];

  std::cout << speach << std::endl;

  // Преобразование запроса в речь
  Httper::ContainerType voice = std::move(http.Get(yaUrl.Make(speach)));

  // Отправка запроса себе же на сервера Telegram
  Httper::ContainerType result =
      std::move(http.Post(tUrl.SendVoice(senderId), "voice", "voice", voice));
  result.push_back(char(0));

  nlohmann::json answer = nlohmann::json::parse(result.data());
  if (answer["ok"].get<bool>() != true) {
    return;
  }

  std::string fileId = answer["result"]["voice"]["file_id"];

  // Формирование и выполнение ответа на запрос
  nlohmann::json cachedVoice;
  cachedVoice["type"] = "voice";
  cachedVoice["id"] = 0;
  cachedVoice["voice_file_id"] = fileId;
  cachedVoice["title"] = speach;

  nlohmann::json queryAnswer;
  queryAnswer = nlohmann::json::array();
  queryAnswer.push_back(cachedVoice);

  Httper::ContainerType data =
      http.Get(tUrl.AnswerInlineQuery(queryId, queryAnswer.dump()));
  data.push_back(0);
  std::cout << "Answer for inline ID: " << queryId << std::endl
            << queryAnswer.dump() << std::endl
            << "Result:" << std::endl
            << data.data() << std::endl;
}