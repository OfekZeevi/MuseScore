/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef MU_PROJECT_IPROJECTCONFIGURATION_H
#define MU_PROJECT_IPROJECTCONFIGURATION_H

#include <QStringList>
#include <QColor>

#include "modularity/imoduleexport.h"
#include "io/path.h"
#include "async/channel.h"
#include "async/notification.h"
#include "inotationproject.h"
#include "projecttypes.h"

namespace mu::project {
class IProjectConfiguration : MODULE_EXPORT_INTERFACE
{
    INTERFACE_ID(IProjectConfiguration)

public:
    virtual ~IProjectConfiguration() = default;

    virtual io::paths_t recentProjectPaths() const = 0;
    virtual void setRecentProjectPaths(const io::paths_t& recentScorePaths) = 0;
    virtual async::Channel<io::paths_t> recentProjectPathsChanged() const = 0;

    virtual io::path_t myFirstProjectPath() const = 0;

    virtual io::paths_t availableTemplateDirs() const = 0;
    virtual io::path_t templateCategoriesJsonPath(const io::path_t& templatesDir) const = 0;

    virtual io::path_t userTemplatesPath() const = 0;
    virtual void setUserTemplatesPath(const io::path_t& path) = 0;
    virtual async::Channel<io::path_t> userTemplatesPathChanged() const = 0;

    virtual io::path_t defaultProjectsPath() const = 0;
    virtual void setDefaultProjectsPath(const io::path_t& path) = 0;

    virtual io::path_t lastOpenedProjectsPath() const = 0;
    virtual void setLastOpenedProjectsPath(const io::path_t& path) = 0;

    virtual io::path_t lastSavedProjectsPath() const = 0;
    virtual void setLastSavedProjectsPath(const io::path_t& path) = 0;

    virtual io::path_t userProjectsPath() const = 0;
    virtual void setUserProjectsPath(const io::path_t& path) = 0;
    virtual async::Channel<io::path_t> userProjectsPathChanged() const = 0;

    virtual io::path_t cloudProjectsPath() const = 0;
    virtual bool isCloudProject(const io::path_t& path) const = 0;

    virtual io::path_t defaultSavingFilePath(INotationProjectPtr project,
                                             const QString& filenameAddition = QString(), const QString& suffix = QString()) const = 0;

    virtual bool shouldAskSaveLocationType() const = 0;
    virtual void setShouldAskSaveLocationType(bool shouldAsk) = 0;

    virtual SaveLocationType lastUsedSaveLocationType() const = 0;
    virtual void setLastUsedSaveLocationType(SaveLocationType type) = 0;

    virtual bool shouldWarnBeforePublishing() const = 0;
    virtual void setShouldWarnBeforePublishing(bool shouldWarn) = 0;

    virtual QColor templatePreviewBackgroundColor() const = 0;
    virtual async::Notification templatePreviewBackgroundChanged() const = 0;

    enum class PreferredScoreCreationMode {
        FromInstruments,
        FromTemplate
    };

    virtual PreferredScoreCreationMode preferredScoreCreationMode() const = 0;
    virtual void setPreferredScoreCreationMode(PreferredScoreCreationMode mode) = 0;

    virtual MigrationOptions migrationOptions(MigrationType type) const = 0;
    virtual void setMigrationOptions(MigrationType type, const MigrationOptions& opt, bool persistent = true) = 0;

    virtual bool isAutoSaveEnabled() const = 0;
    virtual void setAutoSaveEnabled(bool enabled) = 0;
    virtual async::Channel<bool> autoSaveEnabledChanged() const = 0;

    virtual int autoSaveIntervalMinutes() const = 0;
    virtual void setAutoSaveInterval(int minutes) = 0;
    virtual async::Channel<int> autoSaveIntervalChanged() const = 0;

    virtual io::path_t newProjectTemporaryPath() const = 0;

    virtual bool isAccessibleEnabled() const = 0;

    virtual bool shouldDestinationFolderBeOpenedOnExport() const = 0;
    virtual void setShouldDestinationFolderBeOpenedOnExport(bool shouldDestinationFolderBeOpenedOnExport) = 0;
};
}

#endif // MU_PROJECT_IPROJECTCONFIGURATION_H
