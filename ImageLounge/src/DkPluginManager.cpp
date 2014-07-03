/*******************************************************************************************************
 DkPluginManager.cpp
 Created on:	20.05.2013

 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances

 Copyright (C) 2011-2013 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2013 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2013 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#include "DkPluginManager.h"


namespace nmc {

/**********************************************************************************
* Plugin manager dialog
**********************************************************************************/
DkPluginManager::DkPluginManager(QWidget* parent, Qt::WindowFlags flags) : QDialog(parent, flags) {

	init();
}

DkPluginManager::~DkPluginManager() {

}

/**
* initialize plugin manager dialog - set sizes
 **/
void DkPluginManager::init() {
	
	loadedPlugins = QMap<QString, DkPluginInterface *>();
	pluginLoaders = QMap<QString, QPluginLoader *>();
	pluginFiles = QMap<QString, QString>();
	pluginIdList = QList<QString>();
	runId2PluginId = QMap<QString, QString>();
	loadEnabledPlugins(); //pluginLoadingDebuging -> comment this line

	dialogWidth = 700;
	dialogHeight = 500;

	setWindowTitle(tr("Plugin manager"));
	setFixedSize(dialogWidth, dialogHeight);
	createLayout();

}

/*
* create plugin manager dialog layout
 **/
void DkPluginManager::createLayout() {
	
	QVBoxLayout* verticalLayout = new QVBoxLayout(this);
	tabs = new QTabWidget(this);

	tableWidgetInstalled = new DkPluginTableWidget(tab_installed_plugins, this, tabs->currentWidget());
	tabs->addTab(tableWidgetInstalled, tr("Manage installed plug-ins"));
	tableWidgetDownload = new DkPluginTableWidget(tab_download_plugins, this, tabs->currentWidget());
	tabs->addTab(tableWidgetDownload, tr("Download new plug-ins"));
	connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
    verticalLayout->addWidget(tabs);
	
    QHBoxLayout* horizontalLayout = new QHBoxLayout();
    QSpacerItem* horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    horizontalLayout->addItem(horizontalSpacer);

	QPushButton* buttonClose = new QPushButton(tr("&Close"));
	connect(buttonClose, SIGNAL(clicked()), this, SLOT(closePressed()));
	buttonClose->setDefault(true);

    horizontalLayout->addWidget(buttonClose);

    verticalLayout->addLayout(horizontalLayout);

	this->setSizeGripEnabled(false);
}


void DkPluginManager::closePressed() {

	this->close();
}

void DkPluginManager::showEvent(QShowEvent *event) {

	loadPlugins();

	tabs->setCurrentIndex(tab_installed_plugins);
	tableWidgetInstalled->clearTableFilters();
	tableWidgetInstalled->updateInstalledModel();
	tableWidgetDownload->clearTableFilters();
}

void DkPluginManager::deleteInstance(QString id) {

	if (pluginLoaders.contains(id)) {

		pluginFiles.remove(id);
		for (int i = 0; i < pluginIdList.size(); i++) 
			if (pluginIdList.at(i) == id) {
				pluginIdList.removeAt(i);
				break;
			}
		loadedPlugins.remove(id);

		QPluginLoader* loaderToDelete = pluginLoaders.take(id);
		if(!loaderToDelete->unload()) qDebug() << "Could not unload plug-in loader!";
		delete loaderToDelete;
		loaderToDelete = 0;
	}
}

/**
* Loads one plug-in from file fileName
* @param fileName
 **/
void DkPluginManager::singlePluginLoad(QString filePath) {

	QPluginLoader* loader = new QPluginLoader(filePath);
	
	if (!loader->load()) {
        qDebug() << "Could not load: " << filePath;
		return;
    }
	
	QObject* pluginObject = loader->instance();

	if(pluginObject) {

		DkPluginInterface* initializedPlugin = qobject_cast<DkPluginInterface*>(pluginObject);

		if (!initializedPlugin)
			initializedPlugin = qobject_cast<DkViewPortInterface*>(pluginObject);

		if(initializedPlugin) {
			QString pluginID = initializedPlugin->pluginID();
			pluginIdList.append(pluginID);
			loadedPlugins.insert(pluginID, initializedPlugin);
			pluginLoaders.insert(pluginID, loader);
			pluginFiles.insert(pluginID, filePath);
		}
		else
			qDebug() << "could not initialize: " << filePath;
	}
	else {
		delete loader;
		qDebug() << "could not load: " << filePath;
	}
}


/**
* Loads/reloads installed plug-ins
 **/
void DkPluginManager::loadPlugins() {

	// if reloading first delete all instances
	if (!pluginLoaders.isEmpty()) {

		for(int i = 0; i < pluginIdList.count(); i++) {

			QPluginLoader* pluginLoader = pluginLoaders.take(pluginIdList.at(i));
			qDebug() << pluginLoader->errorString();
			
			if(!pluginLoader->unload()) qDebug() << "Could not unload plug-in loader!";
			
			delete pluginLoader;
			pluginLoader = 0;

		}
	}

	pluginFiles.clear();
	runId2PluginId.clear();	
	loadedPlugins.clear();
	pluginIdList.clear();
	pluginLoaders.clear();

	QDir pluginsDir = DkSettings::global.pluginsDir;
	//QDir pluginsDir = QDir(qApp->applicationDirPath());
    //pluginsDir.cd("plugins");

	foreach(QString fileName, pluginsDir.entryList(QDir::Files)) singlePluginLoad(pluginsDir.absoluteFilePath(fileName));

	QSettings settings;
	int i = 0;

	settings.remove("PluginSettings/filePaths");
	settings.beginWriteArray("PluginSettings/filePaths");
	for (int j = 0; j < pluginIdList.size(); j++) {
		settings.setArrayIndex(i++);
		settings.setValue("pluginId", pluginIdList.at(j));
		settings.setValue("pluginFilePath", pluginFiles.value(pluginIdList.at(j)));
	}
	settings.endArray();
}

//Loads enabled plug-ins when the menu is first hit
void DkPluginManager::loadEnabledPlugins() {

	if (!loadedPlugins.isEmpty()) qDebug() << "This should be empty!";

	QMap<QString, QString> pluginsPaths = QMap<QString, QString>();
	QList<QString> disabledPlugins = QList<QString>();
	QSettings settings;

	int size = settings.beginReadArray("PluginSettings/filePaths");
	for (int i = 0; i < size; i++) {
		settings.setArrayIndex(i);
		pluginsPaths.insert(settings.value("pluginId").toString(), settings.value("pluginFilePath").toString());
	}
	settings.endArray();

	size = settings.beginReadArray("PluginSettings/disabledPlugins");
	for (int i = 0; i < size; i++) {
		settings.setArrayIndex(i);
		disabledPlugins.append(settings.value("pluginId").toString());
	}
	settings.endArray();

	QMapIterator<QString, QString> iter(pluginsPaths);	

	while(iter.hasNext()) {
		iter.next();
		/*if (!disabledPlugins.contains(iter.key()))*/ singlePluginLoad(iter.value());
	}
}

//returns map with id and interface
QMap<QString, DkPluginInterface*> DkPluginManager::getPlugins() {

	return loadedPlugins;
}

DkPluginInterface* DkPluginManager::getPlugin(QString key) {
	
	DkPluginInterface* cPlugin = loadedPlugins.value(getRunId2PluginId().value(key));

	// if we could not find the runID, try to see if it is a pluginID
	if (!cPlugin)
		cPlugin = loadedPlugins.value(key);
	
	return cPlugin;
}

QList<QString> DkPluginManager::getPluginIdList() {

	return pluginIdList;
}

QMap<QString, QString> DkPluginManager::getPluginFileNames() {

	return pluginFiles;
}

void DkPluginManager::setPluginIdList(QList<QString> newPlugins) {

	pluginIdList = newPlugins;
}

void DkPluginManager::setRunId2PluginId(QMap<QString, QString> newMap) {

	runId2PluginId = newMap;
}

QMap<QString, QString> DkPluginManager::getRunId2PluginId(){

	return runId2PluginId;
}

void DkPluginManager::tabChanged(int tab){
	
	if(tab == tab_installed_plugins) tableWidgetInstalled->updateInstalledModel();
	else if(tab == tab_download_plugins) tableWidgetDownload->downloadPluginInformation(xml_usage_download);
}

void DkPluginManager::deletePlugin(QString pluginID) {

	QPluginLoader* loaderToDelete = pluginLoaders.take(pluginID);
		
	if(!loaderToDelete->unload()) qDebug() << "Could not unload plug-in loader!";
	delete loaderToDelete;
	loaderToDelete = 0;

	QFile file(pluginFiles.take(pluginID));
	if(!file.remove()) {
		qDebug() << "Failed to delete plug-in file!";
		QMessageBox::critical(this, tr("Plugin manager"), tr("The dll could not be deleted!\nPlease restart nomacs and try again."));
	}

	loadedPlugins.remove(pluginID);
}

//*********************************************************************************
//DkPluginTableWidget : Widget with table views containing plugin data
//*********************************************************************************

DkPluginTableWidget::DkPluginTableWidget(int tab, DkPluginManager* manager, QWidget* parent) : QWidget(parent) {

	pluginDownloader = new DkPluginDownloader(this);
	openedTab = tab;
	pluginManager = manager;
	createLayout();
}

DkPluginTableWidget::~DkPluginTableWidget() {

}


// create the main layout of the plugin manager
void DkPluginTableWidget::createLayout() {

	QVBoxLayout* verticalLayout = new QVBoxLayout(this);

	// search line edit and update button
	QHBoxLayout* searchHorLayout = new QHBoxLayout();
	QLabel* searchLabel = new QLabel(tr("&Search plug-ins: "), this);
	searchHorLayout->addWidget(searchLabel);
	filterEdit = new QLineEdit(this);
	filterEdit->setFixedSize(160,20);
	connect(filterEdit, SIGNAL(textChanged(QString)), this, SLOT(filterTextChanged()));
	searchLabel->setBuddy(filterEdit);
	searchHorLayout->addWidget(filterEdit);
	QSpacerItem* horizontalSpacer;
	if(openedTab == tab_installed_plugins) horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
	else horizontalSpacer = new QSpacerItem(40, 23, QSizePolicy::Expanding, QSizePolicy::Minimum);
	searchHorLayout->addItem(horizontalSpacer);
	if(openedTab == tab_installed_plugins) {
		QPushButton *updateButton = new QPushButton("&Update plug-ins", this);
		connect(updateButton, SIGNAL(clicked()),this, SLOT(updatePlugins()));
		updateButton->setFixedWidth(120);
		searchHorLayout->addWidget(updateButton);
	}
	verticalLayout->addLayout(searchHorLayout);

	// main table
    tableView = new QTableView(this);
	proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setDynamicSortFilter(true);
	//tableView->setMaximumHeight(100);
	if(openedTab == tab_installed_plugins) model = new DkInstalledPluginsModel(pluginManager->getPluginIdList(), this);
	else if (openedTab == tab_download_plugins) model = new DkDownloadPluginsModel(this);
	proxyModel->setSourceModel(model);
	tableView->setModel(proxyModel);
	tableView->resizeColumnsToContents();
	if(openedTab == tab_installed_plugins) {
		tableView->setColumnWidth(ip_column_name, qMax(tableView->columnWidth(ip_column_name), 300));
		tableView->setColumnWidth(ip_column_version, qMax(tableView->columnWidth(ip_column_version), 80));
		tableView->setColumnWidth(ip_column_enabled, qMax(tableView->columnWidth(ip_column_enabled), 130));
	} else if (openedTab == tab_download_plugins) {
		tableView->setColumnWidth(dp_column_name, qMax(tableView->columnWidth(dp_column_name), 360));
		tableView->setColumnWidth(dp_column_version, qMax(tableView->columnWidth(dp_column_version), 80));
		//tableView->setColumnWidth(ip_column_enabled, qMax(tableView->columnWidth(ip_column_enabled), 130));
	}

	tableView->resizeRowsToContents();	
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->setSortingEnabled(true);
	tableView->sortByColumn(ip_column_name, Qt::AscendingOrder);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->verticalHeader()->hide();
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
	tableView->setAlternatingRowColors(true);
	//tableView->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
	if(openedTab == tab_installed_plugins) {
		tableView->setItemDelegateForColumn(ip_column_enabled, new DkPluginCheckBoxDelegate(tableView));
		DkPushButtonDelegate* buttonDelegate = new DkPushButtonDelegate(tableView);
		tableView->setItemDelegateForColumn(ip_column_uninstall, buttonDelegate);
		connect(buttonDelegate, SIGNAL(buttonClicked(QModelIndex)), this, SLOT(uninstallPlugin(QModelIndex)));
		connect(pluginDownloader, SIGNAL(allPluginsUpdated(bool)), this, SLOT(pluginUpdateFinished(bool)));
	} else if (openedTab == tab_download_plugins) {
		DkDownloadDelegate* buttonDelegate = new DkDownloadDelegate(tableView);
		tableView->setItemDelegateForColumn(dp_column_install, buttonDelegate);
		connect(buttonDelegate, SIGNAL(buttonClicked(QModelIndex)), this, SLOT(installPlugin(QModelIndex)));
		connect(pluginDownloader, SIGNAL(pluginDownloaded(const QModelIndex &)), this, SLOT(pluginInstalled(const QModelIndex &)));
	}
    verticalLayout->addWidget(tableView);

	QSpacerItem* verticalSpacer = new QSpacerItem(40, 15, QSizePolicy::Expanding, QSizePolicy::Minimum);
	verticalLayout->addItem(verticalSpacer);

	QVBoxLayout* bottomVertLayout = new QVBoxLayout();

	// additional information
	QHBoxLayout* topHorLayout = new QHBoxLayout();
	QLabel* descLabel = new QLabel(tr("Plug-in description:"));
	topHorLayout->addWidget(descLabel);
	QLabel* previewLabel = new QLabel(tr("Plug-in preview:"));
	topHorLayout->addWidget(previewLabel);
	bottomVertLayout->addLayout(topHorLayout);

	QHBoxLayout* bottHorLayout = new QHBoxLayout();
	int layoutHWidth = this->geometry().width();
	int layoutHWidth2 = this->contentsRect().width();

	//QTextEdit* decriptionEdit = new QTextEdit();
	//decriptionEdit->setReadOnly(true);
	//bottHorLayout->addWidget(decriptionEdit);
	DkDescriptionEdit* decriptionEdit = new DkDescriptionEdit(model, proxyModel, tableView->selectionModel(), this);
	connect(tableView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), decriptionEdit, SLOT(selectionChanged(const QItemSelection &, const QItemSelection &)));
	connect(proxyModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), decriptionEdit, SLOT(dataChanged(const QModelIndex &, const QModelIndex &)));
	bottHorLayout->addWidget(decriptionEdit);

	//QLabel* imgLabel = new QLabel();
	//imgLabel->setMinimumWidth(layoutHWidth/2 + 4);
	//imgLabel->setPixmap(QPixmap::fromImage(QImage(":/nomacs/img/imgDescriptionMissing.png")));	
	DkDescriptionImage* decriptionImg = new DkDescriptionImage(model, proxyModel, tableView->selectionModel(), this);
	//decriptionImg->setMinimumWidth(324);//layoutHWidth/2 + 4);
	//decriptionImg->setMaximumWidth(324);//layoutHWidth/2 + 4);
	decriptionImg->setMaximumSize(324,168);
	decriptionImg->setMinimumSize(324,168);
	connect(tableView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), decriptionImg, SLOT(selectionChanged(const QItemSelection &, const QItemSelection &)));
	connect(proxyModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), decriptionImg, SLOT(dataChanged(const QModelIndex &, const QModelIndex &)));
	connect(pluginDownloader, SIGNAL(imageDownloaded(QImage)), decriptionImg, SLOT(updateImageFromReply(QImage)));
	bottHorLayout->addWidget(decriptionImg);
	bottomVertLayout->addLayout(bottHorLayout);

	verticalLayout->addLayout(bottomVertLayout);
}

void DkPluginTableWidget::updatePlugins() {

	downloadPluginInformation(xml_usage_update);
}

void DkPluginTableWidget::manageParsedXmlData(int usage) {

	if (usage == xml_usage_download) fillDownloadTable();
	else if (usage == xml_usage_update) updateSelectedPlugins();
	else showDownloaderMessage("Sorry, too many connections at once.", tr("Plug-in manager"));
}

void DkPluginTableWidget::updateSelectedPlugins() {

	DkInstalledPluginsModel* installedPluginsModel = static_cast<DkInstalledPluginsModel*>(model);
	QList<QString> installedIdList = installedPluginsModel->getPluginData();
	QList<XmlPluginData> updateList = pluginDownloader->getXmlPluginData();
	pluginsToUpdate = QList<XmlPluginData>();
	
	for (int i = 0; i < updateList.size(); i++) {
		for (int j = 0; j < installedIdList.size(); j++) {
			if(updateList.at(i).id == installedIdList.at(j)) {
				if(updateList.at(i).version != pluginManager->getPlugins().value(installedIdList.at(j))->pluginVersion()) pluginsToUpdate.append(updateList.at(i));
				break;
			}
		}
	}

	if (pluginsToUpdate.size() > 0) {

		QStringList pluginsNames = QStringList();
		for (int i = 0; i < pluginsToUpdate.size(); i++) pluginsNames.append(pluginsToUpdate.at(i).name + "    v:" + pluginsToUpdate.at(i).version);

		QMessageBox msgBox;
		msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		msgBox.setDefaultButton(QMessageBox::No);
		msgBox.setEscapeButton(QMessageBox::No);
		msgBox.setIcon(QMessageBox::Question);
		msgBox.setWindowTitle(tr("Update plug-ins"));

		msgBox.setText(tr("There are updates available for next plug-ins:<br><i>%1</i><br><br>Do you want to update?").arg(pluginsNames.join("<br>")));
		
		if(msgBox.exec() == QMessageBox::Yes) {

			QList<QString> updatedList = pluginManager->getPluginIdList();
			for (int i = 0; i < pluginsToUpdate.size(); i++) {
				for (int j = updatedList.size() - 1; j >= 0; j--) 
					if (updatedList.at(j) == pluginsToUpdate.at(i).id) {
						updatedList.removeAt(j);
						break;
				}
			}
			pluginManager->setPluginIdList(updatedList);
			updateInstalledModel(); // before deleting instance remove entries from table

			for (int i = 0; i < pluginsToUpdate.size(); i++) pluginManager->deleteInstance(pluginsToUpdate.at(i).id);

			// after deleting instances the file are not in use anymore -> update
			QList<QString> urls = QList<QString>();
			while (pluginsToUpdate.size() > 0) {

#if defined _WIN64
				QString downloadUrl = pluginsToUpdate.takeLast().downloadX64;
#elif _WIN32
				QString downloadUrl = pluginsToUpdate.takeLast().downloadX86;
#endif
				urls.append(downloadUrl);
			}

			pluginDownloader->updatePlugins(urls);
		}

		msgBox.deleteLater();

	}
	else showDownloaderMessage(tr("All plug-ins are up to date."), tr("Plug-in manager"));
}

void DkPluginTableWidget::pluginUpdateFinished(bool finishedSuccessfully) {

	pluginManager->loadPlugins();
	updateInstalledModel();
	if (finishedSuccessfully) showDownloaderMessage(tr("The plug-ins have been updated."), tr("Plug-in manager"));
}

void DkPluginTableWidget::filterTextChanged() {

	Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive; //filterCaseSensitivityCheckBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;

	QRegExp regExp(filterEdit->text(), caseSensitivity, QRegExp::FixedString);
	proxyModel->setFilterRegExp(regExp);
	tableView->resizeRowsToContents();
}

void DkPluginTableWidget::uninstallPlugin(const QModelIndex &index) {

	DkInstalledPluginsModel* installedPluginsModel = static_cast<DkInstalledPluginsModel*>(model);
	int selectedRow = proxyModel->mapToSource(index).row();
	QString pluginID = installedPluginsModel->getPluginData().at(selectedRow);

	QMessageBox msgBox;
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	msgBox.setDefaultButton(QMessageBox::No);
	msgBox.setEscapeButton(QMessageBox::No);
	msgBox.setIcon(QMessageBox::Question);
	msgBox.setWindowTitle(tr("Uninstall plug-ins"));

	msgBox.setText(tr("Do you really want to uninstall the plug-in <i>%1</i>?").arg(pluginManager->getPlugins().value(pluginID)->pluginName()));

	if(msgBox.exec() == QMessageBox::Yes) {

		QMap<QString, bool> enabledSettings = installedPluginsModel->getEnabledData();
		QList<QString> pluginIdList = installedPluginsModel->getPluginData();	
		pluginIdList.removeAt(selectedRow);			
		enabledSettings.remove(pluginID);

		pluginManager->setPluginIdList(pluginIdList);
		installedPluginsModel->setEnabledData(enabledSettings);
		installedPluginsModel->savePluginsEnabledSettings();
		updateInstalledModel();	// !!! update model before deleting the interface

		pluginManager->deletePlugin(pluginID);
	}
	msgBox.deleteLater();
}

void DkPluginTableWidget::installPlugin(const QModelIndex &index) {

	DkDownloadPluginsModel* downloadPluginsModel = static_cast<DkDownloadPluginsModel*>(model);
	QModelIndex sourceIndex = proxyModel->mapToSource(index);
	int selectedRow = sourceIndex.row();
	
#if defined _WIN64
	QString downloadUrl = downloadPluginsModel->getPluginData().at(selectedRow).downloadX64;
#elif _WIN32
	QString downloadUrl = downloadPluginsModel->getPluginData().at(selectedRow).downloadX86;
#endif

	//QDir pluginsDir = QDir(qApp->applicationDirPath());
    //pluginsDir.mkdir("plugins");
	QDir pluginsDir = DkSettings::global.pluginsDir;
	
	if (!pluginsDir.exists())
		pluginsDir.mkpath(pluginsDir.absolutePath());

	qDebug() << "path: " << DkSettings::global.pluginsDir;

	pluginDownloader->downloadPlugin(sourceIndex, downloadUrl);	
}

void DkPluginTableWidget::pluginInstalled(const QModelIndex &index) {

	DkDownloadPluginsModel* downloadPluginsModel = static_cast<DkDownloadPluginsModel*>(model);
	downloadPluginsModel->updateInstalledData(index, true);
	pluginManager->loadPlugins();
}

void DkPluginTableWidget::clearTableFilters(){

	filterEdit->clear();
	filterEdit->setFocus();
}


//update models if new plug-ins are installed or copied into the folder
void DkPluginTableWidget::updateInstalledModel() {

	DkInstalledPluginsModel* installedPluginsModel = static_cast<DkInstalledPluginsModel*>(model);
	installedPluginsModel->loadPluginsEnabledSettings();

	QList<QString> newPluginList = pluginManager->getPluginIdList();
	QList<QString> tableList = installedPluginsModel->getPluginData();

	for (int i = tableList.size() - 1; i >= 0; i--) {
		if (!newPluginList.contains(tableList.at(i))) installedPluginsModel->removeRows(i, 1);
	}

	tableList = installedPluginsModel->getPluginData();

	for (int i = newPluginList.size() - 1; i >= 0; i--) {
		if (!tableList.contains(newPluginList.at(i))) {
			installedPluginsModel->setDataToInsert(newPluginList.at(i));
			installedPluginsModel->insertRows(installedPluginsModel->getPluginData().size(), 1);
		}
	}

	tableView->resizeRowsToContents();
}

void DkPluginTableWidget::downloadPluginInformation(int usage) {

	pluginDownloader->downloadXml(usage);
}

void DkPluginTableWidget::fillDownloadTable() {

	QList<XmlPluginData> xmlPluginData = pluginDownloader->getXmlPluginData();
	DkDownloadPluginsModel* downloadPluginsModel = static_cast<DkDownloadPluginsModel*>(model);
	QList<XmlPluginData> modelData = downloadPluginsModel->getPluginData();
	QMap<QString, bool> installedPlugins = downloadPluginsModel->getInstalledData();

	// delete rows if needed
	if (modelData.size() > 0) {
		for (int i = modelData.size() - 1; i >= 0; i--) {
			int j;
			for (j = 0; j < xmlPluginData.size(); j++) if (xmlPluginData.at(j).id == modelData.at(i).id && xmlPluginData.at(j).decription == modelData.at(i).decription
				&& xmlPluginData.at(j).previewImgUrl == modelData.at(i).previewImgUrl && xmlPluginData.at(j).name == modelData.at(i).name &&
				xmlPluginData.at(j).version == modelData.at(i).version) break;
			if (j >= xmlPluginData.size()) downloadPluginsModel->removeRows(i, 1);
		}
	}

	modelData = downloadPluginsModel->getPluginData(); //refresh model data after removing rows

	// insert new rows if needed
	if (modelData.size() > 0) {
		for (int i = 0; i < xmlPluginData.size(); i++) {
			int j;
			for (j = 0; j < modelData.size(); j++) if (xmlPluginData.at(i).id == modelData.at(j).id) break;
			if (j >= modelData.size()) {
				downloadPluginsModel->setDataToInsert(xmlPluginData.at(i));
				downloadPluginsModel->insertRows(downloadPluginsModel->getPluginData().size(), 1);
			}
		}
	}
	else {
		for (int i = 0; i < xmlPluginData.size(); i++) {
			downloadPluginsModel->setDataToInsert(xmlPluginData.at(i));
			downloadPluginsModel->insertRows(downloadPluginsModel->getPluginData().size(), 1);
		}
	}

	// check if installed
	modelData.clear();
	modelData = downloadPluginsModel->getPluginData();	
	QList<QString> pluginIdList = pluginManager->getPluginIdList();

	for (int i = 0; i < modelData.size(); i++) {
		if (pluginIdList.contains(modelData.at(i).id)) downloadPluginsModel->updateInstalledData(downloadPluginsModel->index(i, dp_column_install), true);
		else downloadPluginsModel->updateInstalledData(downloadPluginsModel->index(i, dp_column_install), false);
	}

	tableView->resizeRowsToContents();
}

void DkPluginTableWidget::showDownloaderMessage(QString msg, QString title) {
	QMessageBox infoDialog(this);
	infoDialog.setWindowTitle(title);
	infoDialog.setIcon(QMessageBox::Information);
	infoDialog.setText(msg);
	infoDialog.show();

	infoDialog.exec();
}

DkPluginManager* DkPluginTableWidget::getPluginManager() {

	return pluginManager;
}

int DkPluginTableWidget::getOpenedTab() {

	return openedTab;
}

DkPluginDownloader* DkPluginTableWidget::getDownloader() {

	return pluginDownloader;
}

//**********************************************************************************
//DkInstalledPluginsModel : Model managing installed plug-ins data in the table
//**********************************************************************************

DkInstalledPluginsModel::DkInstalledPluginsModel(QObject *parent) : QAbstractTableModel(parent) {

}

DkInstalledPluginsModel::DkInstalledPluginsModel(QList<QString> data, QObject *parent) : QAbstractTableModel(parent) {

	parentTable = static_cast<DkPluginTableWidget*>(parent);

	pluginData = data;
	pluginsEnabled = QMap<QString, bool>();

	loadPluginsEnabledSettings();
}

int DkInstalledPluginsModel::rowCount(const QModelIndex &parent) const {

	return pluginData.size();
}

int DkInstalledPluginsModel::columnCount(const QModelIndex &parent) const {

	return ip_column_size;
}

QVariant DkInstalledPluginsModel::data(const QModelIndex &index, int role) const {

    if (!index.isValid()) return QVariant();

    if (index.row() >= pluginData.size() || index.row() < 0) return QVariant();

    if (role == Qt::DisplayRole) {
		
		QString pluginID = pluginData.at(index.row());
		QList<QString> fsd = parentTable->getPluginManager()->getPluginIdList();
        if (index.column() == ip_column_name) {
			return parentTable->getPluginManager()->getPlugins().value(pluginID)->pluginName();
		}
        else if (index.column() == ip_column_version) {
			return parentTable->getPluginManager()->getPlugins().value(pluginID)->pluginVersion();
		}
        else if (index.column() == ip_column_enabled)
			return pluginsEnabled.value(pluginID, true);
		else if (index.column() == ip_column_uninstall)
			return QString(tr("Uninstall"));
    }
	
    return QVariant();
}

QVariant DkInstalledPluginsModel::headerData(int section, Qt::Orientation orientation, int role) const {

    if (role != Qt::DisplayRole) return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case ip_column_name:
                return tr("Name");
            case ip_column_version:
                return tr("Version");
            case ip_column_enabled:
                return tr("Enabled/Disabled");
			case ip_column_uninstall:
				return tr("Uninstall plug-in");
            default:
                return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags DkInstalledPluginsModel::flags(const QModelIndex &index) const {

    if (!index.isValid())
        return Qt::ItemIsEnabled;

	return QAbstractTableModel::flags(index);
}

bool DkInstalledPluginsModel::setData(const QModelIndex &index, const QVariant &value, int role) {

	if (index.isValid() && role == Qt::EditRole) {
        
		if (index.column() == ip_column_enabled) {
			pluginsEnabled.insert(pluginData.at(index.row()), value.toBool());
			savePluginsEnabledSettings();

			emit(dataChanged(index, index));

			return true;
		}
    }

    return false;
}

bool DkInstalledPluginsModel::insertRows(int position, int rows, const QModelIndex &index) {

    beginInsertRows(QModelIndex(), position, position+rows-1);

    for (int row=0; row < rows; row++) {
		pluginData.insert(position, dataToInsert);
    }

    endInsertRows();
    return true;
}

bool DkInstalledPluginsModel::removeRows(int position, int rows, const QModelIndex &index) {

    beginRemoveRows(QModelIndex(), position, position+rows-1);

	for (int row=0; row < rows; row++) {
        pluginData.removeAt(position+row);
    }

    endRemoveRows();
    return true;
}

QList<QString> DkInstalledPluginsModel::getPluginData() {
	
	return pluginData;
}

void DkInstalledPluginsModel::setDataToInsert(QString newData) {

	dataToInsert = newData;
}

void DkInstalledPluginsModel::setEnabledData(QMap<QString, bool> enabledData) {

	pluginsEnabled = enabledData;
}

QMap<QString, bool> DkInstalledPluginsModel::getEnabledData() {

	return pluginsEnabled;
}

void DkInstalledPluginsModel::loadPluginsEnabledSettings() {

	pluginsEnabled.clear();

	QSettings settings;
	int size = settings.beginReadArray("PluginSettings/disabledPlugins");
	for (int i = 0; i < size; i++) {

		settings.setArrayIndex(i);
		pluginsEnabled.insert(settings.value("pluginId").toString(), false);
	}
	settings.endArray();
}

void DkInstalledPluginsModel::savePluginsEnabledSettings() {
	
	QSettings settings;
	settings.remove("PluginSettings/disabledPlugins");
	
	if (pluginsEnabled.size() > 0) {

		int i = 0;
		QMapIterator<QString, bool> iter(pluginsEnabled);	

		settings.beginWriteArray("PluginSettings/disabledPlugins");
		while(iter.hasNext()) {
			iter.next();
			if (!iter.value()) {
				settings.setArrayIndex(i++);
				settings.setValue("pluginId", iter.key());
			}
		}
		settings.endArray();
	}
}


//**********************************************************************************
//DkDownloadPluginsModel : Model managing the download plug.in data
//**********************************************************************************

DkDownloadPluginsModel::DkDownloadPluginsModel(QObject *parent) : QAbstractTableModel(parent) {

	parentTable = static_cast<DkPluginTableWidget*>(parent);

	pluginData = QList<XmlPluginData>();
	pluginsInstalled = QMap<QString, bool>();
}

int DkDownloadPluginsModel::rowCount(const QModelIndex &parent) const {

	return pluginData.size();
}

int DkDownloadPluginsModel::columnCount(const QModelIndex &parent) const {

	return dp_column_size;
}

QVariant DkDownloadPluginsModel::data(const QModelIndex &index, int role) const {

    if (!index.isValid()) return QVariant();

    if (index.row() >= pluginData.size() || index.row() < 0) return QVariant();

    if (role == Qt::DisplayRole) {
 
		if (index.column() == dp_column_name) {
			return pluginData.at(index.row()).name;
		}
        else if (index.column() == dp_column_version) {
			return pluginData.at(index.row()).version;
		}
		else if (index.column() == dp_column_install)
			return QString(tr("Download and Install"));
    }

	if (role == Qt::UserRole) {
		return pluginsInstalled.value(pluginData.at(index.row()).id, false);
	}
	
    return QVariant();
}

QVariant DkDownloadPluginsModel::headerData(int section, Qt::Orientation orientation, int role) const {

    if (role != Qt::DisplayRole) return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case dp_column_name:
                return tr("Name");
            case dp_column_version:
                return tr("Version");
			case dp_column_install:
				return tr("Download and install plug-in");
            default:
                return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags DkDownloadPluginsModel::flags(const QModelIndex &index) const {

    if (!index.isValid())
        return Qt::ItemIsEnabled;

	return QAbstractTableModel::flags(index);
}

bool DkDownloadPluginsModel::setData(const QModelIndex &index, const QVariant &value, int role) {

    return false;
}

bool DkDownloadPluginsModel::insertRows(int position, int rows, const QModelIndex &index) {

    beginInsertRows(QModelIndex(), position, position+rows-1);

    for (int row=0; row < rows; row++) {
		pluginData.insert(position, dataToInsert);
    }

    endInsertRows();
    return true;
}

bool DkDownloadPluginsModel::removeRows(int position, int rows, const QModelIndex &index) {

    beginRemoveRows(QModelIndex(), position, position+rows-1);

    for (int row=0; row < rows; row++) {
        pluginData.removeAt(position+row);
    }

    endRemoveRows();
    return true;
}

void DkDownloadPluginsModel::updateInstalledData(const QModelIndex &index, bool installed) {

	if(installed) pluginsInstalled.insert(pluginData.at(index.row()).id, true);
	else pluginsInstalled.remove(pluginData.at(index.row()).id);
	emit dataChanged(index, index);
}

QList<XmlPluginData> DkDownloadPluginsModel::getPluginData() {
	
	return pluginData;
}

void DkDownloadPluginsModel::setDataToInsert(XmlPluginData newData) {

	dataToInsert = newData;
}

void DkDownloadPluginsModel::setInstalledData(QMap<QString, bool> installedData) {

	pluginsInstalled = installedData;
}

QMap<QString, bool> DkDownloadPluginsModel::getInstalledData() {

	return pluginsInstalled;
}

//*********************************************************************************
//DkCheckBoxDelegate : delagete for checkbox only column in the model
//*********************************************************************************

static QRect CheckBoxRect(const QStyleOptionViewItem &viewItemStyleOptions) {

	QStyleOptionButton checkBoxStyleOption;
	QRect checkBoxRect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkBoxStyleOption);
	QPoint checkBoxPoint(viewItemStyleOptions.rect.x() + viewItemStyleOptions.rect.width() / 2 - checkBoxRect.width() / 2,
                         viewItemStyleOptions.rect.y() + viewItemStyleOptions.rect.height() / 2 - checkBoxRect.height() / 2);
	return QRect(checkBoxPoint, checkBoxRect.size());
}

DkPluginCheckBoxDelegate::DkPluginCheckBoxDelegate(QObject *parent) : QStyledItemDelegate(parent) {
	
	parentTable = static_cast<QTableView*>(parent);
}

void DkPluginCheckBoxDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

	if (option.state & QStyle::State_Selected) {
		if (parentTable->hasFocus()) painter->fillRect(option.rect, option.palette.highlight());
		else  painter->fillRect(option.rect, option.palette.background());
	}
	//else if (index.row() % 2 == 1) painter->fillRect(option.rect, option.palette.alternateBase());	// already done automatically

	bool checked = index.model()->data(index, Qt::DisplayRole).toBool();

	QStyleOptionButton checkBoxStyleOption;
	checkBoxStyleOption.state |= QStyle::State_Enabled;
	if (checked) {
		checkBoxStyleOption.state |= QStyle::State_On;
	} else {
		checkBoxStyleOption.state |= QStyle::State_Off;
	}
	checkBoxStyleOption.rect = CheckBoxRect(option);

	QApplication::style()->drawControl(QStyle::CE_CheckBox, &checkBoxStyleOption, painter);
}

bool DkPluginCheckBoxDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
	
	if ((event->type() == QEvent::MouseButtonRelease) || (event->type() == QEvent::MouseButtonDblClick)) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
		if (mouseEvent->button() != Qt::LeftButton || !CheckBoxRect(option).contains(mouseEvent->pos())) {
			return false;
		}
		if (event->type() == QEvent::MouseButtonDblClick) {
			return true;
		}
	} 
	else if (event->type() == QEvent::KeyPress) {
		if (static_cast<QKeyEvent*>(event)->key() != Qt::Key_Space && static_cast<QKeyEvent*>(event)->key() != Qt::Key_Select) {
			return false;
		}
	} 
	else return false;

	bool checked = index.model()->data(index, Qt::DisplayRole).toBool();
	return model->setData(index, !checked, Qt::EditRole);
}



//*********************************************************************************
//DkPushButtonDelegate : delagete for uninstall column in the model
//*********************************************************************************

static QRect PushButtonRect(const QStyleOptionViewItem &viewItemStyleOptions) {

	
	QRect pushButtonRect = viewItemStyleOptions.rect;
	//pushButtonRect.setHeight(pushButtonRect.height() - 2);
	//pushButtonRect.setWidth(pushButtonRect.width() - 2);
	QPoint pushButtonPoint(viewItemStyleOptions.rect.x() + viewItemStyleOptions.rect.width() / 2 - pushButtonRect.width() / 2,
                         viewItemStyleOptions.rect.y() + viewItemStyleOptions.rect.height() / 2 - pushButtonRect.height() / 2);
	return QRect(pushButtonPoint, pushButtonRect.size());
}

DkPushButtonDelegate::DkPushButtonDelegate(QObject *parent) : QStyledItemDelegate(parent) {
	
	parentTable = static_cast<QTableView*>(parent);
	pushButonState = QStyle::State_Enabled;
	currRow = -1;
}

void DkPushButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

	if (option.state & QStyle::State_Selected) {
		if (parentTable->hasFocus()) painter->fillRect(option.rect, option.palette.highlight());
		else  painter->fillRect(option.rect, option.palette.background());
	}

	QStyleOptionButton pushButtonStyleOption;
	pushButtonStyleOption.text = index.model()->data(index, Qt::DisplayRole).toString();
	if (currRow == index.row()) pushButtonStyleOption.state = pushButonState | QStyle::State_Enabled;
	else pushButtonStyleOption.state = QStyle::State_Enabled;
	pushButtonStyleOption.rect = PushButtonRect(option);

	QApplication::style()->drawControl(QStyle::CE_PushButton, &pushButtonStyleOption, painter);
}

bool DkPushButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
	
	if ((event->type() == QEvent::MouseButtonRelease) || (event->type() == QEvent::MouseButtonPress)) {

		QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
		if (mouseEvent->button() != Qt::LeftButton || !PushButtonRect(option).contains(mouseEvent->pos())) {
			pushButonState = QStyle::State_Raised;
			return false;
		}
	}
	else if (event->type() == QEvent::KeyPress) {
		if (static_cast<QKeyEvent*>(event)->key() != Qt::Key_Space && static_cast<QKeyEvent*>(event)->key() != Qt::Key_Select) {
			pushButonState = QStyle::State_Raised;
			return false;
		}
	} 
	else {
		pushButonState = QStyle::State_Raised;
		return false;
	}

    if(event->type() == QEvent::MouseButtonPress) {
		pushButonState = QStyle::State_Sunken;
		currRow = index.row();
	}
	else if( event->type() == QEvent::MouseButtonRelease) {
		pushButonState = QStyle::State_Raised;
        emit buttonClicked(index);
    }    
    return true;

}


//**********************************************************************************
//DkDownloadDelegate : icon if already downloaded or button if not
//**********************************************************************************

DkDownloadDelegate::DkDownloadDelegate(QObject *parent) : QStyledItemDelegate(parent) {
	
	parentTable = static_cast<QTableView*>(parent);
	pushButonState = QStyle::State_Enabled;
	currRow = -1;
}

void DkDownloadDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

	if (option.state & QStyle::State_Selected) {
		if (parentTable->hasFocus()) painter->fillRect(option.rect, option.palette.highlight());
		else  painter->fillRect(option.rect, option.palette.background());
	}

	if (index.model()->data(index, Qt::UserRole).toBool()) {

		QPixmap installedIcon(16, 16);
		installedIcon.load(":/nomacs/img/downloaded.png", "PNG");

		QStyleOptionViewItem iconOption = option;
		iconOption.displayAlignment = Qt::AlignCenter;
		QPoint installedIconPoint(option.rect.x() + option.rect.width() / 2 - installedIcon.width() / 2,
                         option.rect.y() + option.rect.height() / 2 - installedIcon.height() / 2);
		iconOption.rect = QRect(installedIconPoint, installedIcon.size());
		painter->drawPixmap(iconOption.rect, installedIcon);
	}
	else {

		QStyleOptionButton pushButtonStyleOption;
		pushButtonStyleOption.text = index.model()->data(index, Qt::DisplayRole).toString();
		if (currRow == index.row()) pushButtonStyleOption.state = pushButonState | QStyle::State_Enabled;
		else pushButtonStyleOption.state = QStyle::State_Enabled;
		pushButtonStyleOption.rect = PushButtonRect(option);

		QApplication::style()->drawControl(QStyle::CE_PushButton, &pushButtonStyleOption, painter);
	}
}

bool DkDownloadDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) {
	
	if (index.model()->data(index, Qt::UserRole).toBool()) return false;
	else {
		if ((event->type() == QEvent::MouseButtonRelease) || (event->type() == QEvent::MouseButtonPress)) {

			QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
			if (mouseEvent->button() != Qt::LeftButton || !PushButtonRect(option).contains(mouseEvent->pos())) {
				pushButonState = QStyle::State_Raised;
				return false;
			}
		}
		else if (event->type() == QEvent::KeyPress) {
			if (static_cast<QKeyEvent*>(event)->key() != Qt::Key_Space && static_cast<QKeyEvent*>(event)->key() != Qt::Key_Select) {
				pushButonState = QStyle::State_Raised;
				return false;
			}
		} 
		else {
			pushButonState = QStyle::State_Raised;
			return false;
		}

		if(event->type() == QEvent::MouseButtonPress) {
			pushButonState = QStyle::State_Sunken;
			currRow = index.row();
		}
		else if( event->type() == QEvent::MouseButtonRelease) {
			pushButonState = QStyle::State_Raised;
			emit buttonClicked(index);
		}    
	}
    return true;

}



//**********************************************************************************
//DkDescriptionEdit : text edit connected to tableView selection and models
//**********************************************************************************

DkDescriptionEdit::DkDescriptionEdit(QAbstractTableModel* data, QSortFilterProxyModel* proxy, QItemSelectionModel* selection, QWidget *parent) : QTextEdit(parent) {
	
	parentTable = static_cast<DkPluginTableWidget*>(parent);
	dataModel = data;
	proxyModel = proxy;
	selectionModel = selection;
	defaultString = QString(tr("<i>Select a table row to show the plugin description.</i>"));
	this->setText(defaultString);
	this->setReadOnly(true);
}

void DkDescriptionEdit::updateText() {

	switch(selectionModel->selection().indexes().count())
	{
		case 0:
			this->setText(defaultString);
			break;
		default:
			QString text = QString();
			int row = selectionModel->selection().indexes().first().row();
			QModelIndex sourceIndex = proxyModel->mapToSource(selectionModel->selection().indexes().first());
			QString pluginID = QString();
			if (parentTable->getOpenedTab()==tab_installed_plugins) {
				DkInstalledPluginsModel* installedPluginsModel = static_cast<DkInstalledPluginsModel*>(dataModel);
				pluginID = installedPluginsModel->getPluginData().at(sourceIndex.row());
				if (!pluginID.isNull()) text = parentTable->getPluginManager()->getPlugins().value(pluginID)->pluginDescription();
			}
			else if (parentTable->getOpenedTab()==tab_download_plugins) {
				DkDownloadPluginsModel* downloadPluginsModel = static_cast<DkDownloadPluginsModel*>(dataModel);
				text = downloadPluginsModel->getPluginData().at(sourceIndex.row()).decription;
			}		
			
			if (text.isNull()) text = tr("Wrong plug-in GUID!");
			this->setText(text);
			break;
	}
}

void DkDescriptionEdit::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {

	updateText();
}

void DkDescriptionEdit::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {

	updateText();
}


//**********************************************************************************
//DkDescriptionImage : image label connected to tableView selection and models
//**********************************************************************************

DkDescriptionImage::DkDescriptionImage(QAbstractTableModel* data, QSortFilterProxyModel* proxy, QItemSelectionModel* selection, QWidget *parent) : QLabel(parent) {
	
	parentTable = static_cast<DkPluginTableWidget*>(parent);
	dataModel = data;
	proxyModel = proxy;
	selectionModel = selection;
	defaultImage = QImage(":/nomacs/img/imgDescriptionMissing.png");
	this->setPixmap(QPixmap::fromImage(defaultImage));	
}

void DkDescriptionImage::updateImage() {

	switch(selectionModel->selection().indexes().count())
	{
		case 0:
			this->setPixmap(QPixmap::fromImage(defaultImage));	
			break;
		default:
			int row = selectionModel->selection().indexes().first().row();
			QModelIndex sourceIndex = proxyModel->mapToSource(selectionModel->selection().indexes().first());
			QString pluginID;
			QImage img;
			if (parentTable->getOpenedTab()==tab_installed_plugins) {
				DkInstalledPluginsModel* installedPluginsModel = static_cast<DkInstalledPluginsModel*>(dataModel);
				pluginID = installedPluginsModel->getPluginData().at(sourceIndex.row());
				img = parentTable->getPluginManager()->getPlugins().value(pluginID)->pluginDescriptionImage();
				if (!img.isNull()) this->setPixmap(QPixmap::fromImage(img));
				else this->setPixmap(QPixmap::fromImage(defaultImage));
			}
			else if (parentTable->getOpenedTab()==tab_download_plugins) {
				DkDownloadPluginsModel* downloadPluginsModel = static_cast<DkDownloadPluginsModel*>(dataModel);
				parentTable->getDownloader()->downloadPreviewImg(downloadPluginsModel->getPluginData().at(sourceIndex.row()).previewImgUrl);
			}
			break;
	}
}

void DkDescriptionImage::updateImageFromReply(QImage img) {
	
	if (!img.isNull()) this->setPixmap(QPixmap::fromImage(img));
	else this->setPixmap(QPixmap::fromImage(defaultImage));
}

void DkDescriptionImage::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {

	updateImage();
}

void DkDescriptionImage::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {

	updateImage();
}


//**********************************************************************************
//DkPluginDownloader : load download plug-in data and download new plug-ins
//**********************************************************************************

DkPluginDownloader::DkPluginDownloader(QWidget* parent) {

	reply = 0;
	requestType = request_none;
	downloadAborted = false;
	QList<XmlPluginData> xmlPluginData = QList<XmlPluginData>();
	accessManagerPlugin = new QNetworkAccessManager(this);
	connect(accessManagerPlugin, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)) );
	connect(this, SIGNAL(showDownloaderMessage(QString, QString)), parent, SLOT(showDownloaderMessage(QString, QString)));
	connect(this, SIGNAL(parsingFinished(int)), parent, SLOT(manageParsedXmlData(int)));

	progressDialog = new QProgressDialog("", tr("Cancel Update"), 0, 100, parent);
	connect(progressDialog, SIGNAL(canceled()), this, SLOT(cancelUpdate()));
	connect(this, SIGNAL(pluginDownloaded(const QModelIndex &)), progressDialog, SLOT(hide()));	
	connect(this, SIGNAL(allPluginsUpdated(bool)), progressDialog, SLOT(hide()));
}

void DkPluginDownloader::downloadXml(int usage) {

	currUsage = usage;
	xmlPluginData.clear();
	requestType = request_xml;
	downloadAborted = false;
	reply = accessManagerPlugin->get(QNetworkRequest(QUrl("http://www.nomacs.org/plugins/list.php")));
	QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
}

void DkPluginDownloader::downloadPreviewImg(QString url) {

	requestType = request_preview;
	downloadAborted = false;
	reply = accessManagerPlugin->get(QNetworkRequest(QUrl(url)));
	QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
}

void DkPluginDownloader::downloadPlugin(const QModelIndex &index, QString url) {

	requestType = request_plugin;	
	QStringList splittedUrl = url.split("/");
	fileName = splittedUrl.last();
	pluginIndex = index;
	downloadAborted = false;
	reply = accessManagerPlugin->get(QNetworkRequest(QUrl(url)));
	progressDialog->setLabelText(tr("Downloading plug-in..."));
	progressDialog->show();
	connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(updateDownloadProgress(qint64, qint64)));
	QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

}

void DkPluginDownloader::updatePlugins(QList<QString> urls) {

	fileUpdateMax = urls.size();
	for (int i = 0; i < urls.size(); i++) {

		fileUpdateCurr = i;
		requestType = request_plugin_update;	
		downloadAborted = false;
		QStringList splittedUrl = urls.at(i).split("/");
		fileName = splittedUrl.last();
		
		reply = accessManagerPlugin->get(QNetworkRequest(QUrl(urls.at(i))));
		progressDialog->setLabelText(tr("Updating plug-ins..."));
		progressDialog->show();
		connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(updateDownloadProgress(qint64, qint64)));
		QEventLoop loop;
		connect(this, SIGNAL(pluginUpdated()), &loop, SLOT(quit()));
		loop.exec();
		if (downloadAborted) {
			progressDialog->hide();
			qDebug() << "update aborted";
			emit allPluginsUpdated(false); //even if aborted all plug-ins must be reloaded so the deleted ones reload
			break;
		}
	}
}

void DkPluginDownloader::replyFinished(QNetworkReply* reply) {

	if(!downloadAborted) {
		if (reply->error() != QNetworkReply::NoError) {
			if (requestType == request_xml) emit showDownloaderMessage(tr("Sorry, I could not download plug-in information."), tr("Plug-in manager"));
			else if (requestType == request_preview) emit showDownloaderMessage(tr("Sorry, I could not download plug-in preview."), tr("Plug-in manager"));
			else if (requestType == request_plugin || requestType == request_plugin_update) emit showDownloaderMessage(tr("Sorry, I could not download plug-in."), tr("Plug-in manager"));
			cancelUpdate();
			return;
		}

		switch(requestType) {
			case request_xml:
				parseXml(reply);
				break;
			case request_preview:
				replyToImg(reply);
				break;
			case request_plugin:
				startPluginDownload(reply);
				break;
			case request_plugin_update:
				startPluginDownload(reply);
				break;
		}
	}
}

void DkPluginDownloader::parseXml(QNetworkReply* reply) {
	
	qDebug() << "xml with plug-in data downloaded, starting parsing";

	QXmlStreamReader xml(reply->readAll());

	while(!xml.atEnd() && !xml.hasError()) {

		QXmlStreamReader::TokenType token = xml.readNext();
		if(token == QXmlStreamReader::StartElement) {

			if(xml.name() == "Plugin") {
				QXmlStreamAttributes xmlAttributes = xml.attributes();
				XmlPluginData data;
				data.id = xmlAttributes.value("id").toString();
				data.name = xmlAttributes.value("name").toString();
				data.version = xmlAttributes.value("version").toString();
				data.decription = xmlAttributes.value("description").toString();
				data.downloadX64 = xmlAttributes.value("download_x64").toString();
				data.downloadX86 = xmlAttributes.value("download_x86").toString();	
				data.downloadSrc = xmlAttributes.value("download_src").toString();
				data.previewImgUrl = xmlAttributes.value("preview_url").toString();
				xmlPluginData.append(data);
			}
		}
	}

	if(xml.hasError())  {
		emit showDownloaderMessage(tr("Sorry, I could not parse the downloaded plug-in data xml"), tr("Plug-in manager"));
		xml.clear();
		return;
	}
	xml.clear();

	emit parsingFinished(currUsage);
}

void DkPluginDownloader::replyToImg(QNetworkReply* reply) {

    QByteArray imgData = reply->readAll();
	QImage downloadedImg;
	downloadedImg.loadFromData(imgData);
	emit imageDownloaded(downloadedImg);
}

void DkPluginDownloader::startPluginDownload(QNetworkReply* reply) {

	//QDir pluginsDir = QDir(qApp->applicationDirPath());
    //pluginsDir.cd("plugins
	QDir pluginsDir = DkSettings::global.pluginsDir;

	QFile file(pluginsDir.absolutePath().append("\\").append(fileName));
	if (file.exists()) {
		if(!file.remove())	qDebug() << "Failed to delete plug-in file!";
	}

	if (!file.open(QIODevice::WriteOnly)) {
		emit showDownloaderMessage(tr("Sorry, the plug-in could not be saved."), tr("Plug-in manager"));
		cancelUpdate();
        return;
	}
    bool writeSuccess = (file.write(reply->readAll()) > 0);
	file.close();

	if (writeSuccess) {
		if (requestType == request_plugin) emit showDownloaderMessage(tr("Plug-in %1 was successfully installed.").arg(fileName), tr("Plug-in manager"));
	}
	else {
		emit showDownloaderMessage(tr("Sorry, the plug-in could not be saved."), tr("Plug-in manager"));
		cancelUpdate();
		return;
	}

	if (requestType == request_plugin) emit pluginDownloaded(pluginIndex);
	else if (requestType == request_plugin_update) { 
		if(fileUpdateCurr + 1 < fileUpdateMax) emit pluginUpdated();
		else emit allPluginsUpdated(true);
	}
	else emit showDownloaderMessage("Sorry, too many connections at once.", tr("Plug-in manager"));
}

QList<XmlPluginData> DkPluginDownloader::getXmlPluginData() {

	return xmlPluginData;
}

void DkPluginDownloader::cancelUpdate()  {

	reply->abort(); 
	downloadAborted = true;
	if (requestType == request_plugin_update) emit pluginUpdated(); //stop the loop
}

void DkPluginDownloader::updateDownloadProgress(qint64 received, qint64 total) { 

	if (requestType != request_plugin_update) {
		progressDialog->setMaximum(total);
		progressDialog->setValue(received);
	}
	else {
		progressDialog->setMaximum(100000);
		progressDialog->setValue((int)(100000.0/fileUpdateMax*((double)(received)/total + (double)(fileUpdateCurr)/fileUpdateMax)));
	}

}

};
