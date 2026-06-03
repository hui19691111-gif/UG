#include "ZiDonCuTuDrawing.hpp"

#include "ZiDonCuTuSupport.hpp"

void SetLayer230Selectable()
{
	std::vector<NXOpen::Layer::StateInfo> stateArray(1);
	stateArray[0] = NXOpen::Layer::StateInfo(230, NXOpen::Layer::StateSelectable);
	workPart->Layers()->ChangeStates(stateArray, false);
}

NXOpen::NXObject* CreateTemplateDrawingSheet(bool useNamedBodyTemplateSet)
{
	NXOpen::Drawings::DraftingDrawingSheet* nullSheet(NULL);
	NXOpen::Drawings::DraftingDrawingSheetBuilder* builder =
		workPart->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(nullSheet);
	builder->SetProjectionAngle(NXOpen::Drawings::DrawingSheetBuilder::SheetProjectionAngleFirst);

	const bool hasName =
		workPart->HasUserAttribute("名称", NXOpen::NXObject::AttributeType::AttributeTypeString, -1) &&
		std::string(workPart->GetStringAttribute("名称").GetLocaleText()).length() > 0;

	std::string templatePath;
	if (useNamedBodyTemplateSet)
	{
		templatePath = PluginPath("DATA", hasName ? "A4-noviews-template.prt" : "A4-noviews-template-.prt");
	}
	else
	{
		templatePath = PluginPath("DATA", hasName ? "A4-noviews-template1.prt" : "A4-noviews-template2.prt");
	}
	builder->SetMetricSheetTemplateLocation(templatePath.c_str());

	NXOpen::NXObject* sheetObject = builder->Commit();
	builder->Destroy();
	workPart->Drafting()->SetTemplateInstantiationIsComplete(true);
	theSession->CleanUpFacetedFacesAndEdges();
	return sheetObject;
}

void SetDrawingSheetScale(NXOpen::NXObject* sheetObject, double denominator)
{
	NXOpen::Drawings::DraftingDrawingSheet* sheet =
		dynamic_cast<NXOpen::Drawings::DraftingDrawingSheet*>(sheetObject);
	if (sheet == NULL)
	{
		return;
	}
	NXOpen::Drawings::DraftingDrawingSheetBuilder* builder =
		workPart->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(sheet);
	builder->SetStandardMetricScale(NXOpen::Drawings::DrawingSheetBuilder::SheetStandardMetricScaleCustom);
	builder->SetScaleNumerator(1.0);
	builder->SetScaleDenominator(denominator);
	builder->Commit();
	builder->Destroy();
}

bool SetDrawingSheetScaleFromObject(NXOpen::NXObject* sheetObject, double denominator)
{
	NXOpen::Drawings::DraftingDrawingSheet* sheet =
		dynamic_cast<NXOpen::Drawings::DraftingDrawingSheet*>(sheetObject);
	if (sheet == NULL)
	{
		return false;
	}
	NXOpen::Drawings::DraftingDrawingSheetBuilder* builder =
		workPart->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(sheet);
	builder->SetStandardMetricScale(NXOpen::Drawings::DrawingSheetBuilder::SheetStandardMetricScaleCustom);
	builder->SetScaleNumerator(1.0);
	builder->SetScaleDenominator(denominator);
	builder->Commit();
	builder->Destroy();
	return true;
}

void LoadCurrentDrawingSheetScale()
{
	WorkSheet = workPart->DrawingSheets()->CurrentDrawingSheet();
	WorkSheet->GetScale(&numerator, &SheelScale1);
	SheelScale = SheelScale1 / numerator;
}

NXOpen::Drawings::BaseViewBuilder* CreateScaledBaseViewBuilder(NXOpen::Drawings::BaseView* baseView, bool useNewSheetScale)
{
	NXOpen::Drawings::BaseViewBuilder* builder =
		workPart->DraftingViews()->CreateBaseViewBuilder(baseView);
	if (useNewSheetScale)
	{
		builder->Scale()->SetNumerator(1);
		builder->Scale()->SetDenominator(20);
	}
	else
	{
		builder->Scale()->SetNumerator(numerator);
		builder->Scale()->SetDenominator(SheelScale1);
	}
	return builder;
}

NXOpen::NXObject* CommitBaseViewAt(NXOpen::Drawings::BaseViewBuilder* builder, const NXOpen::Point3d& point)
{
	builder->Placement()->Placement()->SetValue(NULL, workPart->Views()->WorkView(), point);
	NXOpen::NXObject* viewObject = builder->Commit();
	builder->Destroy();
	return viewObject;
}

NXOpen::Drawings::BaseView* CommitBaseViewAtAsBaseView(NXOpen::Drawings::BaseViewBuilder* builder, const NXOpen::Point3d& point)
{
	return dynamic_cast<NXOpen::Drawings::BaseView*>(CommitBaseViewAt(builder, point));
}

NXOpen::NXObject* MoveBaseViewTo(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point)
{
	NXOpen::Drawings::BaseViewBuilder* builder =
		workPart->DraftingViews()->CreateBaseViewBuilder(baseView);
	return CommitBaseViewAt(builder, point);
}

NXOpen::NXObject* MoveBaseViewToCurrentSheetScale(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point)
{
	NXOpen::Drawings::BaseViewBuilder* builder =
		workPart->DraftingViews()->CreateBaseViewBuilder(baseView);
	builder->Scale()->SetNumerator(numerator);
	builder->Scale()->SetDenominator(SheelScale1);
	return CommitBaseViewAt(builder, point);
}

NXOpen::Drawings::BaseView* MoveBaseViewToAsBaseView(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point)
{
	return dynamic_cast<NXOpen::Drawings::BaseView*>(MoveBaseViewTo(baseView, point));
}

NXOpen::Drawings::BaseView* MoveBaseViewToCurrentSheetScaleAsBaseView(NXOpen::Drawings::BaseView* baseView, const NXOpen::Point3d& point)
{
	return dynamic_cast<NXOpen::Drawings::BaseView*>(MoveBaseViewToCurrentSheetScale(baseView, point));
}
