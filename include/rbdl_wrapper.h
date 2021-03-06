#ifndef rbdl_wrapper_h_INCLUDED
#define rbdl_wrapper_h_INCLUDED

#include <rbdl/rbdl.h>
#include <rbdl/rbdl_math.h>
#include <rbdl/addons/luamodel/luamodel.h>
#include <rbdl/addons/luamodel/luatables.h>

#include <QString>
#include <QFileInfo>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

class RBDLModelWrapper;


/* This class provides an abstact base for adding data to a loaded model, such as (Animations, Forces, etc.).
 */

class WrapperExtention {
	protected:
		RBDLModelWrapper* model_parent;

	public:
		WrapperExtention();

		void setModelParent(RBDLModelWrapper* model);

		virtual std::string getExtentionName() = 0;
		virtual Qt3DCore::QEntity* getVisual();
		virtual void update(float current_time) = 0; 
};

/* This class provides a wrapper around rbdl models, in order to visualize them with Qt3D
 */

class RBDLModelWrapper : public QObject {
	Q_OBJECT
	private:
		QString model_file;

		Qt3DCore::QEntity* model_render_obj;
		std::map<std::string, Qt3DCore::QEntity*> body_mesh_map;
		std::map<std::string, Qt3DCore::QTransform*> body_transform_map;

		//all loaded extra data is supposed to be loaded as an extention to the model
		std::map<std::string, WrapperExtention*> extentions;
	public:
		LuaTable model_luatable;

		Matrix3_t axis_transform;
		RigidBodyDynamics::Model* rbdl_model;

		RBDLModelWrapper();

		Qt3DCore::QEntity* getRenderObj() { return model_render_obj; }

		Qt3DCore::QEntity* loadFromFile(QString model_file);

		//takes ownership of extention -> only delete via model not where it was created
		void addExtention(WrapperExtention* extention);
		void addVisual(std::string segment_name, Qt3DCore::QEntity *visual);
		void deleteExtention(std::string name);
		bool hasExtention(std::string name);

		void updateKinematics(RigidBodyDynamics::Math::VectorNd Q);
		QString getFileName() { return QFileInfo(model_file).baseName(); }

		int getModelDof();

		void reload();

		QString getModelFile();

	public slots:
		void model_update(float current_time);

	signals:
		void new_extention_added();
		void visual_added(Qt3DCore::QEntity* visual);

};

#endif 

