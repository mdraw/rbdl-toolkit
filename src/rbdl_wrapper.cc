#include "rbdl_wrapper.h"
#include "rbdl/rbdl_errors.h"

#include <QFileInfo>

#include <Qt3DCore/QTransform>
#include <Qt3DRender/QMesh>
#include <Qt3DRender/QAttribute>
#include <Qt3DExtras/QPhongMaterial>

#include "render_util.h"
#include "util.h"

using namespace RigidBodyDynamics;
using namespace RigidBodyDynamics::Math;

RBDLModelWrapper::RBDLModelWrapper(){
	model_render_obj = NULL;
	rbdl_model = NULL;
}


Qt3DCore::QEntity* RBDLModelWrapper::loadFromFile(QString model_file) {
	QFileInfo check_file(model_file);
	// Is it really a file and no directory?
	if (!check_file.exists() || !check_file.isFile()) {
		throw Errors::RBDLInvalidFileError("The file you tried to load does not exists!");
	}

	this->model_file = model_file;
	if (rbdl_model != NULL) {
		delete rbdl_model;
	}
	rbdl_model = new RigidBodyDynamics::Model();

	//loading model into rbdl to check its validity, may throw error
	RigidBodyDynamics::Addons::LuaModelReadFromFile(model_file.toStdString().c_str(), rbdl_model, false);

	auto q = VectorNd::Zero(rbdl_model->q_size);
	if (model_render_obj != NULL) {
		delete model_render_obj;
	}
	model_render_obj = new Qt3DCore::QEntity();

	// load model lua extra to read parameters for rendering
	model_luatable = LuaTable::fromFile(model_file.toStdString().c_str());
	//std::cout << model_luatable.serialize() << std::endl;

	//read axes form model file
	Vector3d axis_front = model_luatable["configuration"]["axis_front"].getDefault(Vector3d(1., 0., 0.)); 
	Vector3d axis_up = model_luatable["configuration"]["axis_up"].getDefault(Vector3d(0., 1., 0.));
	Vector3d axis_right = model_luatable["configuration"]["axis_right"].getDefault(Vector3d(0., 0., 1.));

	axis_transform(0, 0) = axis_front[0];
	axis_transform(1, 0) = axis_front[1];
	axis_transform(2, 0) = axis_front[2];

	axis_transform(0, 1) = axis_right[0];
	axis_transform(1, 1) = axis_right[1];
	axis_transform(2, 1) = axis_right[2];

	axis_transform(0, 2) = axis_up[0];
	axis_transform(1, 2) = axis_up[1];
	axis_transform(2, 2) = axis_up[2];

	unsigned int segments_cnt = model_luatable["frames"].length();

	//create renderable entities for every segment of the model
	for (int i=1; i<=segments_cnt ; i++) {
		std::string segment_name = model_luatable["frames"][i]["name"].get<std::string>();

		Qt3DCore::QEntity* segment_render_node = new Qt3DCore::QEntity();

		//every segment can render multiple visuals
		unsigned int visuals_cnt = model_luatable["frames"][i]["visuals"].length();

		for (int j=1; j<=visuals_cnt; j++) {
			//read visual parameters and transform to correct coordinates
			QString visual_mesh_src = findFile(model_luatable["frames"][i]["visuals"][j]["src"].get<std::string>());
			Vector3d visual_color = model_luatable["frames"][i]["visuals"][j]["color"].getDefault(Vector3d(1., 1., 1.));

			Vector3d visual_scale = model_luatable["frames"][i]["visuals"][j]["scale"].getDefault(Vector3d(1., 1., 1.));
			visual_scale = axis_transform * visual_scale;
			Vector3d visual_dimensions = model_luatable["frames"][i]["visuals"][j]["dimensions"].getDefault(Vector3d(1., 1., 1.));
			visual_dimensions = axis_transform * visual_dimensions;

			Vector3d visual_translate = model_luatable["frames"][i]["visuals"][j]["translate"].getDefault(Vector3d(0., 0., 0.));
			visual_translate = axis_transform * visual_translate; 
			Vector3d visual_center = model_luatable["frames"][i]["visuals"][j]["mesh_center"].getDefault(Vector3d(0., 0., 0.));
			visual_center = axis_transform * visual_center;

			Qt3DCore::QTransform* visual_transform = new Qt3DCore::QTransform;
			visual_transform->setScale3D(QVector3D(visual_dimensions[0] * visual_scale[0], visual_dimensions[1] * visual_scale[1], visual_dimensions[2] * visual_scale[2]));
			visual_transform->setTranslation(QVector3D(visual_center[0], visual_center[1], visual_center[2]));

			Qt3DCore::QEntity* visual_entity = new Qt3DCore::QEntity(segment_render_node);
			visual_entity->setProperty("Scene.ObjGroup", QVariant(QString("Segments"))); 
			Qt3DCore::QEntity* mesh_entity = new Qt3DCore::QEntity(visual_entity);

			//Calculate and set mesh transforms 
			Qt3DCore::QTransform* mesh_transform = new Qt3DCore::QTransform;
			auto rot_axis1 = Vector3d(1., 0., 0.);
			auto rotation = QQuaternion::fromAxisAndAngle(QVector3D(rot_axis1[0], rot_axis1[1], rot_axis1[2]), 90.f);
			float angle;
			Vector3d axis;
			if(model_luatable["frames"][i]["visuals"][j]["rotate"].exists()) {
				angle = model_luatable["frames"][i]["visuals"][j]["rotate"]["angle"].getDefault(0.f);
				axis = model_luatable["frames"][i]["visuals"][j]["rotate"]["axis"].getDefault(Vector3d(1., 0., 0.));
				axis = (axis_transform * axis).normalized();
				rotation = QQuaternion::fromAxisAndAngle(QVector3D(axis[0], axis[1], axis[2]), angle) * rotation;
			}

			QVector3D translation;
			translation[0] = visual_translate[0];
			translation[1] = visual_translate[1];
			translation[2] = visual_translate[2];

			mesh_transform->setRotation(rotation);
			mesh_transform->setTranslation(translation);
		
			Qt3DExtras::QPhongMaterial* visual_material = new Qt3DExtras::QPhongMaterial;
			visual_material->setAmbient(QColor::fromRgbF(visual_color[0], visual_color[1], visual_color[2], 1.));

			Qt3DRender::QMesh* visual_mesh = new Qt3DRender::QMesh;
			visual_mesh->setSource(QUrl::fromLocalFile(visual_mesh_src));

			mesh_entity->addComponent(visual_mesh);
			mesh_entity->addComponent(mesh_transform);
			mesh_entity->addComponent(visual_material);

			visual_entity->addComponent(visual_transform);
		}


		unsigned int body_id = rbdl_model->GetBodyId(segment_name.c_str());
		auto segment_spacial_transform = CalcBodyToBaseCoordinates(*rbdl_model, q, body_id, Vector3d(0., 0., 0.));
		segment_spacial_transform = axis_transform * segment_spacial_transform;
		auto segment_rotation = Quaternion::fromMatrix(CalcBodyWorldOrientation(*rbdl_model, q, body_id));

		Qt3DCore::QTransform* segment_transform = new Qt3DCore::QTransform;
		segment_transform->setTranslation(QVector3D(segment_spacial_transform[0], segment_spacial_transform[1], segment_spacial_transform[2]));
		segment_transform->setRotation(QQuaternion(segment_rotation[3], segment_rotation[0], segment_rotation[1], segment_rotation[2]));

		//std::cout << segment_spacial_transform.transpose() << std::endl;

		segment_render_node->addComponent(segment_transform);
		segment_render_node->setParent(model_render_obj);

		body_mesh_map[segment_name] = segment_render_node;
		body_transform_map[segment_name] = segment_transform;
	}

	auto model_spacial_transform = CalcBodyToBaseCoordinates(*rbdl_model, q, 0, Vector3d(0., 0., 0.));
	auto model_spacial_rotation = Quaternion::fromMatrix(CalcBodyWorldOrientation(*rbdl_model, q, 0));
	//add a constant rotation for rotating object to fit opengl coordinates
	auto rotation = QQuaternion::fromAxisAndAngle(QVector3D(1., 0., 0.), -90.f) * QQuaternion(model_spacial_rotation[3], model_spacial_rotation[0], model_spacial_rotation[1], model_spacial_rotation[2]);

	Qt3DCore::QTransform* model_transform = new Qt3DCore::QTransform;
	model_transform->setRotation(rotation);
	model_transform->setTranslation(QVector3D(model_spacial_transform[0], model_spacial_transform[1], model_spacial_transform[2]));
	model_render_obj->addComponent(model_transform);

	return model_render_obj;
}

void RBDLModelWrapper::updateKinematics(RigidBodyDynamics::Math::VectorNd Q) {
	for (auto it = body_transform_map.begin(); it != body_transform_map.end(); it++) {
		int body_id = rbdl_model->GetBodyId(it->first.c_str());
		
		auto segment_spacial_transform = CalcBodyToBaseCoordinates(*rbdl_model, Q, body_id, Vector3d(0., 0., 0.), true);
		segment_spacial_transform = segment_spacial_transform;
		auto segment_rotation = Quaternion::fromMatrix(CalcBodyWorldOrientation(*rbdl_model, Q, body_id, true));

		Qt3DCore::QTransform* segment_transform = it->second;
		segment_transform->setTranslation(QVector3D(segment_spacial_transform[0], segment_spacial_transform[1], segment_spacial_transform[2]));
		segment_transform->setRotation(QQuaternion(segment_rotation[3], segment_rotation[0], segment_rotation[1], segment_rotation[2]));
	}
}

void RBDLModelWrapper::reload() {
	this->loadFromFile(this->model_file);
}

void RBDLModelWrapper::model_update(float current_time) {
	for (auto it = extentions.begin(); it != extentions.end(); it++) {
		WrapperExtention* extention = it->second;
		extention->update(current_time);
	}
}

void RBDLModelWrapper::addExtention(WrapperExtention* extention) {
	std::string extention_name = extention->getExtentionName();
	extentions[extention_name] = extention;
	extention->setModelParent(this);

	Qt3DCore::QEntity* visual = extention->getVisual();
	if (visual != nullptr) {
		visual->setParent(model_render_obj);
		emit visual_added(visual);
	}

	emit new_extention_added();
}

int RBDLModelWrapper::getModelDof() {
	return rbdl_model->dof_count;
}

QString RBDLModelWrapper::getModelFile() {
	return model_file;
}

void RBDLModelWrapper::addVisual(std::string segment_name, Qt3DCore::QEntity *visual) {
	Qt3DCore::QEntity* segment_entity;
	try {
		segment_entity = body_mesh_map.at(segment_name);
	} catch (std::exception &e){
		return;
	}
	visual->setParent(segment_entity);
	emit visual_added(visual);
}

void WrapperExtention::setModelParent(RBDLModelWrapper* model) {
	model_parent = model;
}

Qt3DCore::QEntity* WrapperExtention::getVisual() {
	return nullptr;
}

WrapperExtention::WrapperExtention() {
	model_parent = NULL;
}

