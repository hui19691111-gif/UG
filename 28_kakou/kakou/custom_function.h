
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

//通过选择实体创建工程图实体。
extern  void custom_Body_To_DraftingBody(
	Body* body1,//选中的实体
	Drawings::DraftingBody* &DraftingBody,//返回创建的工程图实体
	Drawings::BaseView* BaseView1A);//对应的基本视图

//执行布尔运算。
extern void custom_boolean(
	Body* Body1,//工具体
	std::vector<NXOpen::Body*> bodies2,//目标体集合
	Features::Feature::BooleanType BooleanType1,//布尔运算类型
	Features::Feature*& Feature1, //返回生成的特征
	bool aa,//是否保留工具体
	bool bb);//是否保留目标体

//创建包容体。
extern void custom_BaoLonTi(
	std::vector<NXOpen::TaggedObject*> VTaggedObject1,//用于创建包容体的对象
	NXOpen::Features::ToolingBox* &toolingBox1,//返回包容体特征
	Body* &Body1,//返回所选对象所在实体
	Body* &Body2,//返回包容体实体
	NXOpen::Matrix3x3 matrix1);//方位矩阵

//修改包容体间隙。
extern void custom_reBaoLonTi(
	Features::ToolingBox* toolingBox1,//包容体特征
	const char* aa);//新的间隙值

//删除对象。
extern void custom_del(
	vector<NXOpen::TaggedObject*> objects1);//需要删除的对象

//读取指定方位控件的 Matrix3x3。
extern void custom_manip_getMatrix(NXOpen::BlockStyler::SpecifyOrientation* manip0,//方位控件
	NXOpen::Matrix3x3 &matrix1);//返回方位矩阵

//创建方块。
extern void custom_box(NXOpen::Point3d Point3d2,//原点
	NXOpen::Matrix3x3& matrix1,//方位矩阵
	const char* x, const char* y, const char* z,//长、宽、高
	Features::Feature* &Feature1);//返回生成的特征

//修改方块。
extern void custom_rebox(NXOpen::Point3d Point3d2,//原点
	NXOpen::Matrix3x3& matrix1,//方位矩阵
	const char* x, const char* y, const char* z,//长、宽、高
	Features::Feature*& Feature1);//待修改的特征

#endif 
