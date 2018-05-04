

#include <thread>
#include <chrono>
#include <random>


#include "singleton.h"
#include "tagmanager.h"
#include "tag/tagutil.h"
#include "shutil/dsqlitehandle.h"
#include "app/filesignalmanager.h"
#include "controllers/appcontroller.h"
#include "controllers/tagmanagerdaemoncontroller.h"

#include "dfileservices.h"

#include <QMap>
#include <QList>
#include <QDebug>
#include <QVariant>


static QString randomColor() noexcept
{
    std::random_device device{};

    ///###: Choose a random mean between 0 and 6
    std::default_random_engine engine(device());
    std::uniform_int_distribution<int> uniform_dist(0, 6);
    return  Tag::ColorName[uniform_dist(engine)];
}


TagManager::TagManager()
    :QObject{ nullptr }
{
    this->init_connect();
}

QMap<QString, QString> TagManager::getAllTags()
{
    QMap<QString, QVariant> placeholder_container{ { QString{" "}, QVariant{ QList<QString>{ QString{" "} } } } };
    QVariant var{ TagManagerDaemonController::instance()->disposeClientData(placeholder_container, Tag::ActionType::GetAllTags) };
    placeholder_container = var.toMap();

    QMap<QString, QVariant>::const_iterator c_beg{ placeholder_container.cbegin() };
    QMap<QString, QVariant>::const_iterator c_end{ placeholder_container.cend() };
    QMap<QString, QString> string_dual{};

    for(; c_beg != c_end; ++c_beg){
        string_dual[c_beg.key()] = c_beg.value().toString();
    }

    return string_dual;
}

QList<QString> TagManager::getTagsThroughFiles(const QList<DUrl>& files)
{
    QMap<QString, QVariant> string_var{};

    if(!files.isEmpty()){

        for(const DUrl& url : files){
            string_var[url.toLocalFile()] = QVariant{ QList<QString>{} };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(string_var, Tag::ActionType::GetTagsThroughFile) };
        return var.toStringList();
    }

    return QList<QString>{};
}

QMap<QString, QColor> TagManager::getTagColor(const QList<QString>& tags) const
{
    QMap<QString, QColor> tag_and_color{};

    if(!tags.isEmpty()){
        QMap<QString, QVariant> string_var{};

        for(const QString& tag_name : tags){
            string_var[tag_name] = QVariant{ QList<QString>{ QString{" "} } };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(string_var, Tag::ActionType::GetTagsColor) };
        string_var = var.toMap();
        QMap<QString, QVariant>::const_iterator c_beg{ string_var.cbegin() };
        QMap<QString, QVariant>::const_iterator c_end{ string_var.cend() };

        for(; c_beg != c_end; ++c_beg){
            tag_and_color[c_beg.key()] = Tag::NamesWithColors[c_beg.value().toString()];
        }
    }

    return tag_and_color;
}

QString TagManager::getTagColorName(const QString &tag) const
{
    const QMap<QString, QColor> &map = getTagColor({tag});
    const QColor &color = map.value(tag);

    if (!color.isValid())
        return QString();

    return getColorNameByColor(color);
}

QList<QString> TagManager::getFilesThroughTag(const QString& tagName)
{
    QList<QString> file_list{};

    if(!tagName.isEmpty()){
        QMap<QString, QVariant> string_var{ {tagName,  QVariant{QList<QString>{ QString{" "} }}} };
        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(string_var, Tag::ActionType::GetFilesThroughTag) };
        file_list = var.toStringList();
    }

    return file_list;
}

QString TagManager::getTagNameThroughColor(const QColor &color) const
{
    QString tag_name = Tag::ColorsWithNames.value(color.name());

    return Tag::ActualAndFakerName().value(tag_name);
}

QColor TagManager::getColorByColorName(const QString &colorName) const
{
    auto color_map = Tag::ActualAndFakerName();

    for (auto i = color_map.constBegin(); i != color_map.constEnd(); ++i) {
        if (i.value() == colorName) {
            return Tag::NamesWithColors.value(i.key());
        }
    }

    return QColor();
}

QString TagManager::getColorNameByColor(const QColor &color) const
{
    return Tag::ColorsWithNames.value(color.name());
}

QSet<QString> TagManager::allTagOfDefaultColors() const
{
    QSet<QString> tags;

    for (const QString &color : Tag::ColorName) {
        tags << Tag::ActualAndFakerName().value(color);
    }

    return tags;
}

bool TagManager::makeFilesTags(const QList<QString>& tags, const QList<DUrl>& files)
{
    bool result{ true };

    if(!tags.isEmpty() && !files.isEmpty()){
        QMap<QString, QVariant> tag_and_file{};

        for(const QString& tag_name : tags){
            QString color_name;

            // for default tags
            for (const QString &color : Tag::ColorName) {
                if (tag_name == Tag::ActualAndFakerName().value(color)) {
                    color_name = color;
                    break;
                }
            }

            if (color_name.isEmpty())
                color_name = randomColor();

            tag_and_file[tag_name] = QVariant{QList<QString>{ color_name }};
        }

        QVariant insert_tags_var{ TagManagerDaemonController::instance()->disposeClientData(tag_and_file, Tag::ActionType::BeforeMakeFilesTags) };
        QMap<QString, QVariant> file_and_tag{};

        for(const DUrl& url : files){
            file_and_tag[url.toLocalFile()] = QVariant{tags};
        }

        QVariant tag_files_var{};

        if(insert_tags_var.toBool()){
            tag_files_var = TagManagerDaemonController::instance()->disposeClientData(file_and_tag, Tag::ActionType::MakeFilesTags);
        }

        if(insert_tags_var.toBool()){

            if(!tag_files_var.toBool()){
                qWarning()<< "Create tags successfully! But failed to tag files";
            }
            result = true;
        }
    }

    return result;
}

bool TagManager::changeTagColor(const QString& tagName, const QPair<QString, QString>& oldAndNewTagColor)
{
    bool result{ true };

    if(!tagName.isEmpty() && !oldAndNewTagColor.first.isEmpty() && !oldAndNewTagColor.second.isEmpty()){
        QMap<QString, QVariant> string_var{ { tagName, QVariant{ QList<QString>{oldAndNewTagColor.first, oldAndNewTagColor.second} } } };
        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(string_var, Tag::ActionType::ChangeTagColor) };
        result = var.toBool();
    }

    return result;
}

bool TagManager::removeTagsOfFiles(const QList<QString>& tags, const QList<DUrl>& files)
{
    bool result{ true };

    if(!tags.isEmpty() && !files.isEmpty()){
        QMap<QString, QVariant> file_and_tag{};

        for(const DUrl& url : files){
            file_and_tag[url.toLocalFile()] = QVariant(tags);
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(file_and_tag, Tag::ActionType::RemoveTagsOfFiles) };
        result = var.toBool();
    }

    return result;
}

bool TagManager::deleteTags(const QList<QString>& tags)
{
    bool result{ true };

    if(!tags.isEmpty()){
        QMap<QString, QVariant> tag_and_placeholder{};

        for(const QString& tag_name : tags){
            tag_and_placeholder[tag_name] = QVariant{ QList<QString>{} };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(tag_and_placeholder, Tag::ActionType::DeleteTags) };
        result = var.toBool();
    }

    return result;
}

bool TagManager::deleteFiles(const QList<DUrl>& fileList)
{
    bool result{ true };

    if(!fileList.isEmpty()){
        QMap<QString, QVariant> local_url_and_placeholder{};

        for(const DUrl& url : fileList){
            local_url_and_placeholder[url.toLocalFile()] = QVariant{ QList<QString>{} };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(local_url_and_placeholder, Tag::ActionType::DeleteFiles) };
        result = var.toBool();
    }

    return result;
}

void TagManager::init_connect()noexcept
{
    connect(DFileService::instance(), &DFileService::fileCopied, this, [this] (const DUrl &source, const DUrl &target) {
        const QStringList &tags = DFileService::instance()->getTagsThroughFiles(this, {source});

        if (tags.isEmpty())
            return;

        DFileService::instance()->setFileTags(this, target, tags);
    });

    connect(DFileService::instance(), &DFileService::fileDeleted, this, [this] (const DUrl &file) {
        if (file.isLocalFile()) {
            deleteFiles({file});
        }
    });

    connect(DFileService::instance(), &DFileService::fileRenamed, this, [this] (const DUrl &from, const DUrl &to) {
        const QStringList &tags = DFileService::instance()->getTagsThroughFiles(this, {from});

        if (from.isLocalFile()) {
            deleteFiles({from});
        }

        if (tags.isEmpty())
            return;

        DFileService::instance()->setFileTags(this, to, tags);
    });

    QObject::connect(TagManagerDaemonController::instance(), &TagManagerDaemonController::addNewTags,[this](const QVariant& new_tags){

        emit this->addNewTag(new_tags.toStringList());
    });

    QObject::connect(TagManagerDaemonController::instance(), &TagManagerDaemonController::deleteTags, [this](const QVariant& be_deleted_tags){

       emit this->deleteTag(be_deleted_tags.toStringList());
    });

    QObject::connect(TagManagerDaemonController::instance(), &TagManagerDaemonController::changeTagColor, [this](const QVariantMap& old_and_new_color){

       QMap<QString, QString> old_and_new{};
       QMap<QString, QVariant>::const_iterator c_beg{ old_and_new_color.cbegin() };
       QMap<QString, QVariant>::const_iterator c_end{ old_and_new_color.cend() };

       for(; c_beg != c_end; ++c_beg){
           old_and_new[c_beg.key()]=c_beg.value().toString();
       }

       emit this->changeTagColor(old_and_new);
    });

    QObject::connect(TagManagerDaemonController::instance(), &TagManagerDaemonController::changeTagName, [this](const QVariantMap& old_and_new_name){
       QMap<QString, QString> old_and_new{};
       QMap<QString, QVariant>::const_iterator c_beg{ old_and_new_name.cbegin() };
       QMap<QString, QVariant>::const_iterator c_end{ old_and_new_name.cend() };

       for(; c_beg != c_end; ++c_beg){
           old_and_new[c_beg.key()]=c_beg.value().toString();
       }

       emit this->changeTagName(old_and_new);
    });

    QObject::connect(TagManagerDaemonController::instance(), &TagManagerDaemonController::filesWereTagged, [this](const QVariantMap& files_were_tagged){
        QMap<QString, QList<QString>> file_and_tags{};
        QMap<QString, QVariant>::const_iterator the_beg{ files_were_tagged.cbegin() };
        QMap<QString, QVariant>::const_iterator the_end{ files_were_tagged.cend() };

        for(; the_beg != the_end; ++the_beg){
            file_and_tags[the_beg.key()] = the_beg.value().toStringList();
        }

        emit this->filesWereTagged(file_and_tags);
    });

    QObject::connect(TagManagerDaemonController::instance(), &TagManagerDaemonController::untagFiles, [this](const QVariantMap& tag_be_removed_files){
        QMap<QString, QList<QString>> file_and_tags{};
        QMap<QString, QVariant>::const_iterator the_beg{ tag_be_removed_files.cbegin() };
        QMap<QString, QVariant>::const_iterator the_end{ tag_be_removed_files.cend() };

        for(; the_beg != the_end; ++the_beg){
            file_and_tags[the_beg.key()] = the_beg.value().toStringList();
        }

        emit this->untagFiles(file_and_tags);
    });
}


bool TagManager::changeTagName(const QPair<QString, QString>& oldAndNewName)
{
    bool result{ true };

    if(!oldAndNewName.first.isEmpty() && !oldAndNewName.second.isEmpty()){
        QMap<QString, QVariant> tag_name{ {oldAndNewName.first, QVariant{oldAndNewName.second}} };
        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(tag_name, Tag::ActionType::ChangeTagName) };
        result = var.toBool();
    }

    return result;
}

bool TagManager::makeFilesTagThroughColor(const QString& color, const QList<DUrl>& files)
{
    bool result{ true };

    if(!color.isEmpty() && !files.isEmpty()){
        QMap<QString, QVariant> local_url_and_tag{};

        for(const DUrl& url : files){
            local_url_and_tag[url.toLocalFile()] = QVariant{ Tag::ColorsWithNames[color] };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(local_url_and_tag, Tag::ActionType::MakeFilesTagThroughColor) };
        result = var.toBool();
    }

    return result;
}

bool TagManager::changeFilesName(const QList<QPair<DUrl, DUrl>>& oldAndNewFilesName)
{
    bool result{ true };

    if(!oldAndNewFilesName.isEmpty()){
        QMap<QString, QVariant> new_and_old_name{};

        for(const QPair<DUrl, DUrl>& old_new : oldAndNewFilesName){
            new_and_old_name[old_new.first.toLocalFile()] = QVariant{ old_new.second.toLocalFile() };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(new_and_old_name, Tag::ActionType::ChangeFilesName) };
        result = var.toBool();
    }

    return result;
}

