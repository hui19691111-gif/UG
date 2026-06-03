
// custom_function.h
#ifndef CUSTOM_FUNCTION_INCLUDED
#define CUSTOM_FUNCTION_INCLUDED

/***************************************************************************

  ***************************************************************************/
#include "UG_TouWenJian.h"
#include <NXOpen/Drawings_BaseView.hxx>
using namespace std;
using namespace NXOpen;
using namespace NXOpen::BlockStyler;

//通过选择实体，获得视图里的工程图实体
extern  void custom_Body_To_DraftingBody(
	Body* body1,//选择实体
	Drawings::DraftingBody* &DraftingBody,//得到工程图视图里的实体
	Drawings::BaseView* BaseView1A);//相应视图

//创建布尔运算
extern void custom_boolean(
	Body* Body1,//输入工具体
	std::vector<NXOpen::Body*> bodies2,//输入目标体
	Features::Feature::BooleanType BooleanType1,//输入布尔运算类型
	Features::Feature*& Feature1, //输出布尔运算特征
	bool aa,//输入是否保留工具体
	bool bb);//输入是否保留目标体

//创建包容体
extern void custom_BaoLonTi(
	std::vector<NXOpen::TaggedObject*> VTaggedObject1,//包容体所包含的对象	
	NXOpen::Features::ToolingBox* &toolingBox1,//输出包容体特征
	Body* &Body1,//输出所选对象所在的实体
	Body* &Body2,//包容体实体
	NXOpen::Matrix3x3 matrix1);//方位

//修改包容体间隙
extern void custom_reBaoLonTi(
	Features::ToolingBox* toolingBox1,//输入容体特征
	const char* aa);//输入修改间隙

//删除对象
extern void custom_del(
	vector<NXOpen::TaggedObject*> objects1);//输入要删除的对象

//输入指定方位获得Matrix3x3
extern void custom_manip_getMatrix(NXOpen::BlockStyler::SpecifyOrientation* manip0,//输入指定方位
	NXOpen::Matrix3x3 &matrix1);//输出Matrix3x3

//创建方体
extern void custom_box(NXOpen::Point3d Point3d2,//输入指定方位
	NXOpen::Matrix3x3& matrix1,//输入指定方位
	const char* x, const char* y, const char* z,//输入长宽高
	Features::Feature* &Feature1);//输出特征

//修改方体
extern void custom_rebox(NXOpen::Point3d Point3d2,//输入指定方位
	NXOpen::Matrix3x3& matrix1,//输入指定方位
	const char* x, const char* y, const char* z,//输入长宽高
	Features::Feature*& Feature1);//输出特征

#endif 
