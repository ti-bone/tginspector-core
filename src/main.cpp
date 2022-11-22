/*
 * main.cpp
 * Core of TgInspector, main file, do not sell\post it without permission!
 * Copyright (C) ti-bone 2021-2022
 *
 * You can contact me at:
 * Telegram: @CHAKROVIRIVATEL [ID: 322954184]
 * E-Mail: admin@d3v.pp.ua & admin@chakro.lol
 * Jabber: dev@jabber.fr
 */

//tdlib
#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

//boost
#include <boost/lexical_cast.hpp>
#include <boost/exception/diagnostic_information.hpp> 

//std
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <utility>
#include <filesystem>

namespace fs = std::filesystem;

//SQL
#include "sql/sql.hpp"

// Logging
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// overloaded
namespace detail {
    template <class... Fs>
    struct overload;

    template <class F>
    struct overload<F> : public F {
        explicit overload(F f) : F(f) {
        }
    };
    template <class F, class... Fs>
    struct overload<F, Fs...>
            : public overload<F>
                    , public overload<Fs...> {
        [[maybe_unused]] explicit overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
        }
        using overload<F>::operator();
        using overload<Fs...>::operator();
    };
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
    return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;

class Core {
public:
    Core() {
        td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(1));
        client_manager_ = std::make_unique<td::ClientManager>();
        client_id_ = client_manager_->create_client_id();
        send_query(td_api::make_object<td_api::getOption>("version"), {});
    }

//    void sendMessage(std::int64_t id, std::string msgText) {
//        std::istringstream strstream;
//        strstream >> id;
//        strstream.get();
//        auto sendmsg = td_api::make_object<td_api::sendMessage>();
//        sendmsg->chat_id_ = id;
//        auto msg = td_api::make_object<td_api::inputMessageText>();
//        msg->text_ = td_api::make_object<td_api::formattedText>();
//        msg->text_->text_ = std::move(msgText);
//        sendmsg->input_message_content_ = std::move(msg);
//        send_query(std::move(sendmsg), {});
//    }

    void start() {
        // Set online mode for receiving more updates
        send_query(
                td::make_tl_object<td::td_api::setOption>
                        ("online", td_api::make_object<td_api::optionValueBoolean>(true)), {}
        );
        while (true) {
            if (need_restart_) {
                restart();
            }
            else if (!are_authorized_) {
                process_response(client_manager_->receive(10));
            }
            else {
                auto response = client_manager_->receive(0);
                if (response.object) {
                    process_response(std::move(response));
                }
                std::this_thread::sleep_for(std::chrono::nanoseconds(5));
//                std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(30));
            }
        }
    }

private:
    // Read config file
    boost::property_tree::ptree cfg = Config::read_cfg(Config::default_cfg_path);

    using Object = td_api::object_ptr<td_api::Object>;
    std::unique_ptr<td::ClientManager> client_manager_;
    std::int32_t client_id_{ 0 };

    td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
    bool are_authorized_{ false };
    bool need_restart_{ false };
    std::uint64_t current_query_id_{ 0 };
    std::uint64_t authentication_query_id_{ 0 };

    std::map<std::uint64_t, std::function<void(Object)>> handlers_;

    std::map<std::int64_t, td_api::object_ptr<td_api::user>> users_;

    std::map<std::int64_t, std::string> chat_title_;

    void restart() {
        client_manager_.reset();
        *this = Core();
    }

    void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
        auto query_id = next_query_id();
        if (handler) {
            handlers_.emplace(query_id, std::move(handler));
        }
        client_manager_->send(client_id_, query_id, std::move(f));
    }

    void process_response(td::ClientManager::Response response) {
        if (!response.object) {
            return;
        }
        //std::cout << response.request_id << " " << to_string(response.object) << std::endl;
        if (response.request_id == 0) {
            return process_update(std::move(response.object));
        }
        auto it = handlers_.find(response.request_id);
        if (it != handlers_.end()) {
            it->second(std::move(response.object));
            handlers_.erase(it);
        }
    }

    [[nodiscard]] std::string get_full_name(std::int64_t user_id) const {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return "unknown user";
        }
        return it->second->first_name_ + " " + it->second->last_name_;
    }

    [[nodiscard]] std::string get_user_first_name(std::int64_t user_id) const {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return "unknown user";
        }
        return it->second->first_name_;
    }

    [[nodiscard]] std::string get_user_last_name(std::int64_t user_id) const {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return "unknown user";
        }
        return it->second->last_name_;
    }

    [[nodiscard]] std::string get_chat_title(std::int64_t chat_id) const {
        auto it = chat_title_.find(chat_id);
        if (it == chat_title_.end()) return "unknown chat";
        return it->second;
    }

    [[nodiscard]] std::string get_username(std::int64_t user_id) const {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            return "";
        }
        return it->second->username_;
    }

    void setOnline(bool mode) {
        // Set online mode for receiving more updates
        send_query(
                td::make_tl_object<td::td_api::setOption>
                        ("online", td_api::make_object<td_api::optionValueBoolean>(mode)), {}
        );
    }

    void process_update(td_api::object_ptr<td_api::Object> update) {
        // Do downcast for determine type of update
        td_api::downcast_call(
            *update, overloaded(
                [this](td_api::updateAuthorizationState& update_authorization_state) {
                    setOnline(true);
                    authorization_state_ = std::move(update_authorization_state.authorization_state_);
                    on_authorization_state_update();
                },
                [this](td_api::updateNewChat& update_new_chat) {
                    chat_title_[update_new_chat.chat_->id_] = update_new_chat.chat_->title_;
                },
                [this](td_api::updateChatTitle& update_chat_title) {
                    chat_title_[update_chat_title.chat_id_] = update_chat_title.title_;
                },
                [this](td_api::updateUser& update_user) {
                    auto user_id = update_user.user_->id_;
                    users_[user_id] = std::move(update_user.user_);
                },
                [this](td_api::updateNewMessage& update_new_message) {
                    setOnline(true);
                    if (cfg.get<bool>("tdlib.test_mode")) spdlog::set_level(spdlog::level::debug);

                    // Stopwatch
                    spdlog::stopwatch total_handling_sw;

                    // Chat info
                    struct chat_info {
                        td::td_api::int53 id = 0;
                        std::string name;
                    };

                    auto chat = new chat_info();

                    chat->id = update_new_message.message_->chat_id_;

                    // Message info
                    struct message_info {
                        td::td_api::int53 id = 0;
                        std::string timestamp;
                    };

                    auto message = new message_info();

                    message->id = update_new_message.message_->id_ / 1048576;
                    message->timestamp = std::to_string(update_new_message.message_->date_);

                    // File info
                    struct file_info {
                        std::string id;
                        std::string type;
                        td::td_api::int32 local_id{};
                        td::td_api::int53 size{};
                    };

                    auto file = new file_info();

                    // Sender info
                    struct sender_info {
                        std::string full_name;
                        std::string first_name;
                        std::string last_name;
                        std::string username;
                        std::int64_t id = 0;
                    };

                    auto sender = new sender_info();

                    // Geolocation info
                    struct location_info {
                        double latitude;
                        double longitude;
                        double accuracy;
                        int heading;
                    };

                    auto location = new location_info();

                    spdlog::info("Received new message with ID: {}, from chat: {}", message->id, chat->id);
                    spdlog::debug("Resolving message sender...");

                    td_api::downcast_call(*update_new_message.message_->sender_id_, overloaded(
                        [this, &sender, &chat](td_api::messageSenderUser& user) {
                            std::int64_t uid = user.user_id_;
                            sender->id = user.user_id_;
                            sender->first_name = get_user_first_name(user.user_id_);
                            sender->last_name = get_user_last_name(user.user_id_);
                            sender->full_name = get_full_name(user.user_id_);
                            sender->username = get_username(user.user_id_);
                            chat->name = get_chat_title(chat->id);
                            spdlog::debug("Sender resolved, type: User, ID: {}", sender->id);

                            // Check if storage enabled in config
                            if (!cfg.get<bool>("storage.storage_enabled")) {
                                spdlog::warn(
                                        "Avatar of User with ID: {} was skipped, because storage is disabled.",
                                        sender->id
                                );
                            }

                            // Get user avatars
                            send_query(td::make_tl_object<td::td_api::getUserProfilePhotos>(uid, 0, 100),
                    [this, uid](Object object) {
                                // Cast object to chatPhotos
                                auto userPics = td::move_tl_object_as<td::td_api::chatPhotos>
                                (object);

                                // Check if user has any avatar
                                if (userPics->total_count_ <= 0) {
                                    spdlog::info(
                                            "Avatar of User with ID: {} was skipped, because this user has no avatars.",
                                             uid
                                    );
                                    return;
                                }

                                // Init currentPic and picUID variables for comfortable use in future
                                auto currentPic = std::move(userPics->photos_[0]);
                                auto picUID = currentPic->sizes_[0]->photo_->remote_->unique_id_;

                                spdlog::info("Writing user's avatar info into DB, User ID: {}.", uid);

                                // Write info about avatar into DB
                                Sql::writeAvatar(
                                        picUID,
                                        std::to_string(uid),
                                        std::to_string(currentPic->added_date_)
                                );

                                // Check if this file already exists
                                if (!picUID.empty() && Sql::checkFile(picUID)) {
                                    spdlog::warn("Avatar with File ID: {} was skipped because this file already exists.",
                                                 picUID);
                                    return;
                                }

                                // Download avatar file if it isn't exists
                                spdlog::info("<File, Avatar> Downloading file...");
                                spdlog::info(
                                        "<File, Avatar> LocalID: {}, RemoteID: {}, FileType: avatar, FileSize: {}.",
                                        currentPic->sizes_[0]->photo_->id_,
                                        currentPic->sizes_[0]->photo_->remote_->id_,
                                        currentPic->sizes_[0]->photo_->expected_size_
                                );

                                send_query(
                                        td::make_tl_object<td::td_api::downloadFile>
                                         (currentPic->sizes_[0]->photo_->id_, 31, 0, 0, false), {}
                                );
                            });
                        },
                        [this, &chat, &sender](td_api::messageSenderChat& senderChat) {
                            chat->name = get_chat_title(chat->id);
                            // If message is coming from channel - set "[CHANNEL]" prefix to sender's first name
                            sender->first_name = "[CHANNEL] " + get_chat_title(senderChat.chat_id_);
                            sender->id = senderChat.chat_id_;
                            spdlog::debug("Sender resolved, type: Channel, ID: {}", sender->id);
                        }
                    ));

                    std::string text;

                    spdlog::debug("Resolving message type...");
                    if (update_new_message.message_->content_->get_id() == td_api::messageText::ID)
                        text = static_cast<td_api::messageText&>(*update_new_message.message_->content_).text_->text_;
                    else if (update_new_message.message_->content_->get_id() == td::td_api::messagePhoto::ID) {
                        auto& photo = static_cast<td_api::messagePhoto&>(*update_new_message.message_->content_);
                        text = photo.caption_->text_;
                        if (text.empty()) text = "[ PHOTO ]";
                        file->id = photo.photo_->sizes_.back()->photo_->remote_->unique_id_;
                        file->local_id = photo.photo_->sizes_.back()->photo_->id_;
                        file->size = photo.photo_->sizes_.back()->photo_->expected_size_;
                        file->type = "photo";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageVideo::ID) {
                        auto& video = static_cast<td_api::messageVideo&>(*update_new_message.message_->content_);
                        text = video.caption_->text_;
                        if (text.empty()) text = "[ VIDEO ]";
                        file->id = video.video_->video_->remote_->unique_id_;
                        file->local_id = video.video_->video_->id_;
                        file->size = video.video_->video_->expected_size_;
                        file->type = "video";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageDocument::ID) {
                        auto& document = static_cast<td_api::messageDocument&>(*update_new_message.message_->content_);
                        text = document.caption_->text_;
                        if (text.empty()) text = "[ DOCUMENT ]";
                        file->id = document.document_->document_->remote_->unique_id_;
                        file->local_id = document.document_->document_->id_;
                        file->size = document.document_->document_->expected_size_;
                        file->type = "document";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageAnimation::ID) {
                        auto& gif = static_cast<td_api::messageAnimation&>(*update_new_message.message_->content_);
                        text = gif.caption_->text_;
                        if (text.empty()) text = "[ GIF ]";
                        file->id = gif.animation_->animation_->remote_->unique_id_;
                        file->local_id = gif.animation_->animation_->id_;
                        file->size = gif.animation_->animation_->expected_size_;
                        file->type = "animation";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageAudio::ID) {
                        auto& audio = static_cast<td_api::messageAudio&>(*update_new_message.message_->content_);
                        text = audio.caption_->text_;
                        if (text.empty()) text = "[ AUDIO ]";
                        file->id = audio.audio_->audio_->remote_->unique_id_;
                        file->local_id = audio.audio_->audio_->id_;
                        file->size = audio.audio_->audio_->expected_size_;
                        file->type = "audio";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageVoiceNote::ID) {
                        auto& voice = static_cast<td_api::messageVoiceNote&>(*update_new_message.message_->content_);
                        text = voice.caption_->text_;
                        if (text.empty()) text = "[ VOICE ]";
                        file->id = voice.voice_note_->voice_->remote_->unique_id_;
                        file->local_id = voice.voice_note_->voice_->id_;
                        file->size = voice.voice_note_->voice_->expected_size_;
                        file->type = "voice";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageVideoNote::ID) {
                        auto& voice = static_cast<td_api::messageVideoNote&>(*update_new_message.message_->content_);
                        // Force set text, because VideoNote couldn't have a caption and messages without text cannot be written into DB
                        text = "[ EMBED VIDEO ]";
                        file->id = voice.video_note_->video_->remote_->unique_id_;
                        file->local_id = voice.video_note_->video_->id_;
                        file->size = voice.video_note_->video_->expected_size_;
                        file->type = "video_note";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageSticker::ID) {
                        auto& sticker = static_cast<td_api::messageSticker&>(*update_new_message.message_->content_);
                        // Force set text, because Sticker couldn't have a text & messages without text can't be written into DB
                        text = "[ STICKER " + sticker.sticker_->emoji_ + " ]";
                        file->id = sticker.sticker_->sticker_->remote_->unique_id_;
                        file->local_id = sticker.sticker_->sticker_->id_;
                        file->size = sticker.sticker_->sticker_->expected_size_;
                        file->type = "sticker";
                    } else if (update_new_message.message_->content_->get_id() == td::td_api::messageLocation::ID) {
                        auto& loc = static_cast<td_api::messageLocation&>(*update_new_message.message_->content_);
                        text = "";
                        file->type = "location";
                        file->id = "";
                        file->local_id = 0;
                        file->size = 0;
                        location->latitude = loc.location_->latitude_;
                        location->longitude = loc.location_->longitude_;
                        location->accuracy = loc.location_->horizontal_accuracy_;
                        location->heading = loc.heading_;
                        // latitude(double), longitude(double), heading(int), accuracy(double)
                    }

                    if (file->type.empty()) spdlog::debug("Resolved message type: text");
                    else spdlog::debug("Resolved message type: {}", file->type);

                    spdlog::debug("Resolving sender and message type took {} seconds.", total_handling_sw);

                    if (!text.empty() &&
                        !update_new_message.message_->is_outgoing_ &&
                        sender->id != 777000 &&
                        sender->id != 322954184 &&
                        sender->id != 288484447) {
                        spdlog::info("Started processing message.");
                        //TdLib::sendMessage(-1002200644287, "Got message: \n[\nchat:\n chat_id: " + std::to_string(chat_id) + "\n chat_name: " + chat_name + "\n]\n \n[\nmessage_id: " + std::to_string(message_id) + "\n] \n\n[\nfrom:\n id: " + std::to_string(sender_id) + "\n first_name: " + sender_first_name + "\n last_name: " + sender_last_name + "\n] \n\n[\ntext:\n" + text + "\n]\n");

                        // Write message in DB
                        Sql::writeMessage(
                            std::to_string(chat->id),
                            chat->name,
                            std::to_string(message->id),
                            message->timestamp,
                            std::to_string(sender->id),
                            sender->first_name,
                            sender->last_name,
                            text,
                            file->id,
                            file->type
                        );

                        // Check if the user have username, if it has - write id->username in separate table
                        if (!sender->username.empty()) Sql::writeUserName(std::to_string(sender->id), sender->username);

                        // Check if the message have file, if it has - download file and write it in DB later
                        // (on receiving the updateFile update with "is_download_completed_" parameter true)
                        if (!file->id.empty() && !Sql::checkFile(file->id) && cfg.get<bool>("storage.storage_enabled")) {
                            if (file->size >= 1073741824 && cfg.get<bool>("tdlib.test_mode"))
                                spdlog::warn("File size bigger than 1GB and app is in test mode, so it was skipped.");
                            else {
                                spdlog::info("<File> Downloading file...");
                                spdlog::info("<File> LocalID: {}, RemoteID: {}, FileType: {}, FileSize: {}.", file->local_id, file->id, file->type, file->size);
                                send_query(td::make_tl_object<td::td_api::downloadFile>(file->local_id, 31, 0, 0, false), {});
                            }
                        } else if (!file->id.empty() && Sql::checkFile(file->id))
                            spdlog::warn("File with ID: {} was skipped because this file already exists.", file->id);
                        else if (!file->id.empty() && !cfg.get<bool>("storage.storage_enabled"))
                            spdlog::warn("File with ID: {} was skipped because storage is disabled.", file->id);

                        spdlog::info("Processed message with id: {} in chat: {}, from: {}.", message->id, chat->id, sender->id);
                        spdlog::debug("Processing message took {} seconds.", total_handling_sw);
                        //std::cout << "Processed message: \n[\nchat:\n chat_id: " << chat_id << "\n chat_name: " << chat_name << "\n]\n \n[\nmessage_id: " << message_id << "\n] \n\n[\nfrom:\n id: " << sender_id << "\n first_name: " << sender_first_name << "\n last_name: " << sender_last_name << "\n] \n\n[\ntext:\n" << text << "\n]\n" << std::endl;
                        //sendMessage(-1002200644287, "Got message: \n[\nchat:\n chat_id: " << chat_id << "\n chat_name: " << chat_name << "\n]\n \n[\nmessage_id: " << message_id << "\n] \n\n[\nfrom:\n id: " << sender_id << "\n first_name: " << sender_first_name << "\n last_name: " << sender_last_name << "\n] \n\n[\ntext:\n" << text << "\n]\n" << std::endl;, "Got message: \n[\nchat:\n chat_id: " + std::to_string(chat_id) + "\n chat_name: " + chat_name + "\n]\n \n[\nmessage_id: " + std::to_string(message_id) + "\n] \n\n[\nfrom:\n id: " + std::to_string(sender_id) + "\n first_name: " + sender_first_name + "\n last_name: " + sender_last_name + "\n] \n\n[\ntext:\n" + text + "\n]\n" );
                    } else if (file->type == "location" &&
                               !update_new_message.message_->is_outgoing_ &&
                               sender->id != 777000 &&
                               sender->id != 322954184 &&
                               sender->id != 288484447) {
                        spdlog::info("Started processing location.");

                        // Write location in DB
                        Sql::writeLocation(
                                std::to_string(chat->id),
                                chat->name,
                                std::to_string(message->id),
                                message->timestamp,
                                std::to_string(sender->id),
                                sender->first_name,
                                sender->last_name,
                                std::to_string(location->latitude),
                                std::to_string(location->longitude),
                                std::to_string(location->accuracy),
                                std::to_string(location->heading)
                        );

                        // Check if the user have username, if it has - write id->username in separate table
                        if (!sender->username.empty()) Sql::writeUserName(std::to_string(sender->id), sender->username);

                        spdlog::info("Processed location with message id: {} in chat: {}, from: {}.", message->id, chat->id, sender->id);
                        spdlog::debug("Processing location took {} seconds.", total_handling_sw);
                    }
                    delete sender;
                    delete chat;
                    delete location;
                    delete message;
                    delete file;
                    setOnline(false);
                },
                [this](td_api::updateFile& file) {
                    setOnline(true);
                    //std::cout << file.file_->local_->downloaded_size_;
                    if (file.file_->local_->is_downloading_completed_ && cfg.get<bool>("storage.storage_enabled")) {
                        try {
                            const auto storage_location = cfg.get<std::string>("storage.storage_location");
                            const auto storage_absolute_location = absolute(fs::path(storage_location)).string();
                            std::string file_name = storage_absolute_location + "/files/" +
                                                    file.file_->remote_->unique_id_ +
                                                    fs::path(file.file_->local_->path_).extension().string();

                            // Create directory, where files was moved after downloading
                            fs::create_directory(storage_absolute_location);
                            fs::create_directory(storage_absolute_location + "/files");

                            // Move file to storage/files
                            fs::rename(file.file_->local_->path_, file_name);

                            // Change permissions to 755 (Owner - full, Others - read only)
                            fs::permissions(file_name, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read, fs::perm_options::add);

                            // Write info about file in the DB
                            Sql::writeFileInfo(file.file_->remote_->unique_id_, file_name);
                        } catch (...) {
                            std::clog << boost::current_exception_diagnostic_information() << std::endl;
                        }

                        //std::cout << "File with ID " << file.file_->remote_->unique_id_ << " downloaded!" << std::endl;
                        spdlog::info("File with ID {} downloaded!", file.file_->remote_->unique_id_ );
                        setOnline(false);
                    }
                },
                [](auto& update) {}));
    }

    auto create_authentication_query_handler() {
        return [this, id = authentication_query_id_](Object object) {
            if (id == authentication_query_id_) {
                check_authentication_error(std::move(object));
            }
        };
    }

    static auto check_account_handler() {
        return [](Object object) {
            // IDs, that allowed to use with tginspector-core
            std::vector<td::td_api::int53> allowedIDs = { 322954184, 5136050988, 2200654717, 2200507770 };

            // Cast object to td_api::user from update
            auto user = td::move_tl_object_as<td_api::user>(object);

            // Check if current user allowed to use with tginspector-core
            bool isAllowed = false;

            for (td::td_api::int53 id : allowedIDs) {
                if (user->id_ == id) isAllowed = true;
            }

            if (!isAllowed) {
                std::cout << "This account isn't allowed to use with this app, contact @CHAKROVIRIVATEL [ID: 322954184] for more info." << std::endl;
                exit(0);
            }

            spdlog::info("Account with ID {} has been authorized.", user->id_);
        };
    }

    void on_authorization_state_update() {
        authentication_query_id_++;
        td_api::downcast_call(*authorization_state_,
                              overloaded(
                [this](td_api::authorizationStateReady&) {
                    are_authorized_ = true;
                    spdlog::info("Got auth.");
                    send_query(td_api::make_object<td_api::getMe>(), check_account_handler());
                },
                [this](td_api::authorizationStateLoggingOut&) {
                are_authorized_ = false;
                std::cout << "Logging out" << std::endl;
            },
            [](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
                    [this](td_api::authorizationStateClosed &) {
                        are_authorized_ = false;
                        need_restart_ = true;
                        std::cout << "Terminated" << std::endl;
                    },
                    [this](td_api::authorizationStateWaitPhoneNumber &) {
                        std::cout << "Enter phone number: " << std::flush;
                        std::string phone_number;
                        std::cin >> phone_number;
                        send_query(
                                td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                                create_authentication_query_handler());
                    },
                    [this](td_api::authorizationStateWaitEmailAddress &) {
                        std::cout << "Enter email address: " << std::flush;
                        std::string email_address;
                        std::cin >> email_address;
                        send_query(td_api::make_object<td_api::setAuthenticationEmailAddress>(email_address),
                                   create_authentication_query_handler());
                    },
                    [this](td_api::authorizationStateWaitEmailCode &) {
                        std::cout << "Enter email authentication code: " << std::flush;
                        std::string code;
                        std::cin >> code;
                        send_query(td_api::make_object<td_api::checkAuthenticationEmailCode>(
                                           td_api::make_object<td_api::emailAddressAuthenticationCode>(code)),
                                   create_authentication_query_handler());
                    },
                    [this](td_api::authorizationStateWaitCode &) {
                        std::cout << "Enter authentication code: " << std::flush;
                        std::string code;
                        std::cin >> code;
                        send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                                   create_authentication_query_handler());
                    },
                    [this](td_api::authorizationStateWaitRegistration &) {
                        std::string first_name;
                        std::string last_name;
                        std::cout << "Enter your first name: " << std::flush;
                        std::cin >> first_name;
                        std::cout << "Enter your last name: " << std::flush;
                        std::cin >> last_name;
                        send_query(td_api::make_object<td_api::registerUser>(first_name, last_name),
                                   create_authentication_query_handler());
                    },
                    [this](td_api::authorizationStateWaitPassword &) {
                        std::cout << "Enter authentication password: " << std::flush;
                        std::string password;
                        std::getline(std::cin, password);
                        send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                                   create_authentication_query_handler());
                    },
                    [](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
                        std::cout << "Confirm this login link on another device: " << state.link_ << std::endl;
                    },
                    [this](td_api::authorizationStateWaitTdlibParameters&) {
                    auto parameters = td_api::make_object<td_api::setTdlibParameters>();
                    parameters->database_directory_ = cfg.get<std::string>("tdlib.tdata_folder");
                    parameters->use_message_database_ = false;
                    parameters->use_secret_chats_ = true;
                    parameters->api_id_ = cfg.get<td_api::int32>("tdlib.api_id");
                    parameters->api_hash_ = cfg.get<std::string>("tdlib.api_hash");
                    parameters->system_language_code_ = cfg.get<std::string>("tdlib.lang_code");
                    parameters->device_model_ = cfg.get<std::string>("tdlib.device_model");
                    parameters->application_version_ = cfg.get<std::string>("tdlib.app_version");
                    parameters->enable_storage_optimizer_ = true;
                    parameters->use_test_dc_ = cfg.get<bool>("tdlib.test_mode");
                    send_query(std::move(parameters), create_authentication_query_handler());
                }));
    }

    void check_authentication_error(Object object) {
        if (object->get_id() == td_api::error::ID) {
            auto error = td::move_tl_object_as<td_api::error>(object);
            std::cout << "Error: " << to_string(error) << std::flush;
            on_authorization_state_update();
        }
    }

    std::uint64_t next_query_id() {
        return ++current_query_id_;
    }
};

int main() {
    // Read config file
    boost::property_tree::ptree cfg = Config::read_cfg(Config::default_cfg_path);

    spdlog::info("Starting tginspector-core version {}...", cfg.get<std::string>("tdlib.app_version"));
    spdlog::info("Loading tdlib...");
    Core tginscore;
    tginscore.start();
    return 0;
}
