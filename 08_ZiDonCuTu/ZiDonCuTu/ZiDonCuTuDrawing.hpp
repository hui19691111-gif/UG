#ifndef ZIDONCUTU_DRAWING_H_INCLUDED
#define ZIDONCUTU_DRAWING_H_INCLUDED

#include "ZiDonCuTu.hpp"

void SetLayer230Selectable();
NXOpen::NXObject* CreateTemplateDrawingSheet(bool useNamedBodyTemplateSet);
void SetDrawingSheetScale(NXOpen::NXObject* sheetObject, double denominator);
bool SetDrawingSheetScaleFromObject(NXOpen::NXObject* sheetObject, double denominator);
void LoadCurrentDrawingSheetScale();
NXOpen::Drawings::BaseViewBuilder* CreateScaledBaseViewBuilder(NXOpen::Drawings::BaseView* baseView, bool useNewSheetScale);
NXOpen::NXObject* CommitBaseViewAt(NXOpen::Drawings::BaseViewBuilder* builder, const NXOpen::Point3d& point);
NXOpen::Drawings::BaseView* CommitBaseViewAtAsBaseView(NXOpen::Drawings::BaseViewBuilder* builder, const NXOpen::Point3d& point);
NXOpen::NXObject* MoveBaseViewTo(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point);
NXOpen::NXObject* MoveBaseViewToCurrentSheetScale(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point);
NXOpen::Drawings::BaseView* MoveBaseViewToAsBaseView(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point);
NXOpen::Drawings::BaseView* MoveBaseViewToCurrentSheetScaleAsBaseView(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point);

#endif // ZIDONCUTU_DRAWING_H_INCLUDED
