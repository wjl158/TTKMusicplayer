#include "musicqqqueryrecommendrequest.h"
#include "musicsemaphoreloop.h"

MusicQQQueryRecommendRequest::MusicQQQueryRecommendRequest(QObject *parent)
    : MusicQueryRecommendRequest(parent)
{
    m_queryServer = QUERY_QQ_INTERFACE;
}

void MusicQQQueryRecommendRequest::startToSearch(const QString &id)
{
    if(!m_manager)
    {
        return;
    }

    TTK_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(id));
    deleteAll();

    const QUrl &musicUrl = MusicUtils::Algorithm::mdII(QQ_RECOMMEND_URL, false);
    m_searchText = id;
    m_interrupt = true;

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(QQ_UA_URL, ALG_UA_KEY, false).toUtf8());
    MusicObject::setSslConfiguration(&request);

    m_reply = m_manager->get(request);
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicQQQueryRecommendRequest::downLoadFinished()
{
    if(!m_reply || !m_manager)
    {
        deleteAll();
        return;
    }

    TTK_LOGGER_INFO(QString("%1 downLoadFinished").arg(getClassName()));
    Q_EMIT clearAllItems();
    m_musicSongInfos.clear();
    m_interrupt = false;

    if(m_reply->error() == QNetworkReply::NoError)
    {
        const QByteArray &bytes = m_reply->readAll();

        QJson::Parser parser;
        bool ok;
        const QVariant &data = parser.parse(bytes, &ok);
        if(ok)
        {
            QVariantMap value = data.toMap();
            if(value["code"].toInt() == 0 && value.contains("new_song"))
            {
                value = value["new_song"].toMap();
                value = value["data"].toMap();
                const QVariantList &datas = value["song_list"].toList();
                foreach(const QVariant &var, datas)
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicObject::MusicSongInformation musicInfo;
                    foreach(const QVariant &var, value["singer"].toList())
                    {
                        if(var.isNull())
                        {
                            continue;
                        }
                        const QVariantMap &name = var.toMap();
                        musicInfo.m_singerName = MusicUtils::String::illegalCharactersReplaced(name["name"].toString());
                        musicInfo.m_artistId = name["mid"].toString();
                    }
                    musicInfo.m_songName = MusicUtils::String::illegalCharactersReplaced(value["name"].toString());
                    musicInfo.m_timeLength = MusicTime::msecTime2LabelJustified(value["interval"].toInt()*1000);

                    m_rawData["songID"] = value["id"].toString();
                    musicInfo.m_songId = value["mid"].toString();

                    const QVariantMap &albumMap = value["album"].toMap();
                    musicInfo.m_albumId = albumMap["mid"].toString();
                    musicInfo.m_albumName = MusicUtils::String::illegalCharactersReplaced(albumMap["name"].toString());
                    musicInfo.m_lrcUrl = MusicUtils::Algorithm::mdII(QQ_SONG_LRC_URL, false).arg(musicInfo.m_songId);
                    musicInfo.m_smallPicUrl = MusicUtils::Algorithm::mdII(QQ_SONG_PIC_URL, false)
                                              .arg(musicInfo.m_albumId.right(2).left(1))
                                              .arg(musicInfo.m_albumId.right(1)).arg(musicInfo.m_albumId);

                    musicInfo.m_year = value["time_public"].toString();
                    musicInfo.m_discNumber = value["index_cd"].toString();
                    musicInfo.m_trackNumber = value["index_album"].toString();

                    if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;
                    readFromMusicSongAttributePlus(&musicInfo, value["file"].toMap());
                    if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;

                    if(musicInfo.m_songAttrs.isEmpty())
                    {
                        continue;
                    }

                    MusicSearchedItem item;
                    item.m_songName = musicInfo.m_songName;
                    item.m_singerName = musicInfo.m_singerName;
                    item.m_albumName = musicInfo.m_albumName;
                    item.m_time = musicInfo.m_timeLength;
                    item.m_type = mapQueryServerString();
                    Q_EMIT createSearchedItem(item);
                    m_musicSongInfos << musicInfo;
                }
            }
        }
    }

    Q_EMIT downLoadDataChanged(QString());
    deleteAll();
}
