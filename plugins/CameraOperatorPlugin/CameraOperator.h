#ifndef CAMERAOPERATORPLUGIN_H_INCLUDED
#define CAMERAOPERATORPLUGIN_H_INCLUDED

#include <QWidget>
#include <QString>
#include <QMenu>

#include <vector>

#include "toolkit_interfaces.h"

class CameraOperatorPlugin : public QObject, public CoreInterface {
	Q_OBJECT
	Q_INTERFACES(CoreInterface)
	Q_PLUGIN_METADATA(IID CoreInterface_iid FILE "metadata.json")

	public:
		CameraOperatorPlugin();
		virtual ~CameraOperatorPlugin();

		void init(ToolkitApp* app);

	private:
		ToolkitApp* parentApp;

		QMenu camera_menu;

	public slots:
		void setOrthographicView(bool state);
		void setTopView();
		void setSideView();
		void setFrontView();
};

#endif 

