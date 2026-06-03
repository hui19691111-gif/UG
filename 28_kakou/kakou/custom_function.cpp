//custom_function

// Mandatory UF Includes


#include "UG_TouWenJian.h"
using namespace NXOpen;

//选择实体获得工程图实�?
void custom_Body_To_DraftingBody(NXOpen::Body* body1, Drawings::DraftingBody* &DraftingBody1, Drawings::BaseView* BaseView1A)//获得面的外围�?
{
	//获取视图body
	std::vector<Drawings::DraftingBody*>DraftingBodyVector;
	Drawings::DraftingBodyCollection* DraftingBodyCollection1 = BaseView1A->DraftingBodies();
	Drawings::DraftingBodyCollection::iterator Ite1 = DraftingBodyCollection1->begin();
	Drawings::DraftingBody* DraftingBody;
	for (; Ite1 != DraftingBodyCollection1->end(); ++Ite1)
	{
		DraftingBody = (*Ite1);
		DraftingBodyVector.push_back(DraftingBody);
	}
	// 获取视图所有Curve
	for (size_t i = 0; i < DraftingBodyVector.size(); i++)
	{
		std::vector<Drawings::DraftingCurve*>DraftingCurvevector;
		std::vector<NXObject*>NXObject1aa;
		Drawings::DraftingCurve* DraftingCurve;
		Drawings::DraftingCurveCollection* DraftingCurveCollection1 = DraftingBodyVector[i]->DraftingCurves();
		Drawings::DraftingCurveCollection::iterator Ite2 = DraftingCurveCollection1->begin();

		DraftingCurve = (*Ite2);//获得某个BODY的一条曲�?

		//得到要标尺寸的视图BODY

		NXObject1aa = DraftingCurve->GetDraftingCurveInfo()->GetParents();//DraftingCurve父项
		int type, subtype;
		UF_OBJ_ask_type_and_subtype(NXObject1aa[0]->Tag(), &type, &subtype);
		if (subtype == UF_solid_face_subtype)
		{
			NXOpen::Face* Face1a(dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(NXObject1aa[0]->Tag())));
			if (body1 == Face1a->GetBody())
			{
				DraftingBody1 = DraftingBodyVector[i];//得到要标尺寸的视图BODY
				i = DraftingBodyVector.size();
			}
		}
		if (subtype == UF_solid_edge_subtype)
		{
			NXOpen::Edge* edge1a(dynamic_cast<NXOpen::Edge*>(NXOpen::NXObjectManager::Get(NXObject1aa[0]->Tag())));
			if (body1 == edge1a->GetBody())
			{
				DraftingBody1 = DraftingBodyVector[i];//得到要标尺寸的视图BODY
				i = DraftingBodyVector.size();
			}
		}
	}
}

void custom_boolean(Body* Body1, std::vector<NXOpen::Body*> bodies2, Features::Feature::BooleanType BooleanType1, Features::Feature*& Feature1, bool aa, bool bb)
{
    UF_initialize();
    NXOpen::Features::BooleanFeature* nullNXOpen_Features_BooleanFeature(NULL);
    NXOpen::Features::BooleanBuilder* booleanBuilder1;
    booleanBuilder1 = workPart->Features()->CreateBooleanBuilderUsingCollector(nullNXOpen_Features_BooleanFeature);
    NXOpen::GeometricUtilities::BooleanRegionSelect* booleanRegionSelect1;
    booleanRegionSelect1 = booleanBuilder1->BooleanRegionSelect();
    booleanBuilder1->SetCopyTargets(aa);
    booleanBuilder1->SetCopyTools(bb);
    booleanBuilder1->SetOperation(BooleanType1);

    NXOpen::ScCollector* scCollector1a;
    scCollector1a = workPart->ScCollectors()->CreateCollector();
    NXOpen::SelectionIntentRuleOptions* selectionIntentRuleOptions1a;
    selectionIntentRuleOptions1a = workPart->ScRuleFactory()->CreateRuleOptions();
    selectionIntentRuleOptions1a->SetSelectedFromInactive(false);
    std::vector<NXOpen::Body*> bodies1(1);

    bodies1[0] = Body1;
    NXOpen::BodyDumbRule* bodyDumbRule1;

    bodyDumbRule1 = workPart->ScRuleFactory()->CreateRuleBodyDumb(bodies1, true, selectionIntentRuleOptions1a);

    std::vector<NXOpen::SelectionIntentRule*> rules1a(1);

    rules1a[0] = bodyDumbRule1;
    scCollector1a->ReplaceRules(rules1a, false);
    booleanBuilder1->SetTargetBodyCollector(scCollector1a);

    NXOpen::ScCollector* scCollector2;
    scCollector2 = workPart->ScCollectors()->CreateCollector();

    NXOpen::BodyDumbRule* bodyDumbRule2;
    bodyDumbRule2 = workPart->ScRuleFactory()->CreateRuleBodyDumb(bodies2, true, selectionIntentRuleOptions1a);
    delete selectionIntentRuleOptions1a;
    std::vector<NXOpen::SelectionIntentRule*> rules2(1);
    rules2[0] = bodyDumbRule2;
    scCollector2->ReplaceRules(rules2, false);
    booleanBuilder1->SetToolBodyCollector(scCollector2);

    Feature1 = booleanBuilder1->CommitFeature();
    booleanBuilder1->Destroy();
    if (BooleanType1 == 3)
    {
        for (size_t ib = 0; ib < Feature1->GetBodies().size(); ib++)
        {
            UF_OBJ_set_color(Feature1->GetBodies()[ib]->Tag(), 55);
        }
    }

    UF_terminate();
}

void custom_reBaoLonTi(Features::ToolingBox* toolingBox1, const char* aa)
{
    NXOpen::Session::UndoMarkId markId1;
    markId1 = theSession->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "Redefine Feature");
    NXOpen::Features::EditWithRollbackManager* editWithRollbackManager1;
    editWithRollbackManager1 = workPart->Features()->StartEditWithRollbackManager(toolingBox1, markId1);

    NXOpen::Features::ToolingBoxBuilder* toolingBoxBuilder1;
    toolingBoxBuilder1 = workPart->Features()->ToolingFeatureCollection()->CreateToolingBoxBuilder(toolingBox1);
    toolingBoxBuilder1->SetReferenceCsysType(NXOpen::Features::ToolingBoxBuilder::RefCsysTypeAbsoluteinDisplayedPart);
    toolingBoxBuilder1->OffsetPositiveX()->SetFormula(aa);
    toolingBoxBuilder1->OffsetNegativeX()->SetFormula(aa);
    toolingBoxBuilder1->OffsetPositiveY()->SetFormula(aa);
    toolingBoxBuilder1->OffsetNegativeY()->SetFormula(aa);
    toolingBoxBuilder1->OffsetPositiveZ()->SetFormula(aa);
    toolingBoxBuilder1->OffsetNegativeZ()->SetFormula(aa);

    NXOpen::NXObject* nXObject1;
    nXObject1 = toolingBoxBuilder1->Commit();
    toolingBoxBuilder1->Destroy();
    editWithRollbackManager1->UpdateFeature(false);
    editWithRollbackManager1->Stop();
    editWithRollbackManager1->Destroy();
}



static double g_customBaoLonTiOffsets[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static bool g_customBaoLonTiUseGuard = false;

void custom_setBaoLonTiUseGuard(bool useGuard)
{
    g_customBaoLonTiUseGuard = useGuard;
}

void custom_setBaoLonTiOffsets(double positiveX, double negativeX, double positiveY, double negativeY, double positiveZ, double negativeZ)
{
    g_customBaoLonTiOffsets[0] = positiveX;
    g_customBaoLonTiOffsets[1] = negativeX;
    g_customBaoLonTiOffsets[2] = positiveY;
    g_customBaoLonTiOffsets[3] = negativeY;
    g_customBaoLonTiOffsets[4] = positiveZ;
    g_customBaoLonTiOffsets[5] = negativeZ;
}

void custom_BaoLonTi(std::vector<NXOpen::TaggedObject*> VTaggedObject1, NXOpen::Features::ToolingBox* &toolingBox1, Body* &Body1, Body* &Body2, NXOpen::Matrix3x3 matrix1)
{
std::vector<Face*>VFace1;
std::vector<Edge*>VEdge1;
std::vector<Point*>VPoint1;
for (size_t i = 0; i < VTaggedObject1.size(); i++)
{
    tag_t object_id = VTaggedObject1[i]->Tag();
    int type;
    int subtype;
    UF_OBJ_ask_type_and_subtype(object_id, &type, &subtype);

    if (type == 70 && subtype == 2)
    {
        Face* face1 = (dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(VTaggedObject1[i]->Tag())));
        VFace1.push_back(face1);
        Body1 = face1->GetBody();
    }
    if (type == 70 && subtype == 3)
    {
        Edge* Edge1 = (dynamic_cast<NXOpen::Edge*>(NXOpen::NXObjectManager::Get(VTaggedObject1[i]->Tag())));
        VEdge1.push_back(Edge1);
        Body1 = Edge1->GetBody();
    }
    if (type == 2 && subtype == 0)
    {
        Point* point1 = dynamic_cast<NXOpen::Point*>(NXOpen::NXObjectManager::Get(VTaggedObject1[i]->Tag()));
        VPoint1.push_back(point1);
        //获得对象的父�?
        int n_parents; tag_p_t parents;
        UF_SO_ask_parents(VTaggedObject1[i]->Tag(), UF_SO_ASK_ALL_PARENTS, &n_parents, &parents);
        //循环得到父项的类型跟子类�?
        for (size_t ia = 0; ia < n_parents; ia++)
        {
            int type1, subtype1;
            UF_OBJ_ask_type_and_subtype(parents[ia], &type1, &subtype1);
            if (type1 == 70 && subtype1 == 3)
            {

                Edge* Edge1 = (dynamic_cast<NXOpen::Edge*>(NXOpen::NXObjectManager::Get(parents[ia])));
                Body1 = Edge1->GetBody();
            }

        }
    }
}
theSession->ToolingSession()->SetWizardType(1);
NXOpen::Features::ToolingBox* nullNXOpen_Features_ToolingBox(NULL);
NXOpen::Features::ToolingBoxBuilder* toolingBoxBuilder1;
toolingBoxBuilder1 = workPart->Features()->ToolingFeatureCollection()->CreateToolingBoxBuilder(nullNXOpen_Features_ToolingBox);

toolingBoxBuilder1->SetReferenceCsysType(NXOpen::Features::ToolingBoxBuilder::RefCsysTypeAbsoluteinDisplayedPart);
toolingBoxBuilder1->SetType(NXOpen::Features::ToolingBoxBuilder::TypesBoundedBlock);
toolingBoxBuilder1->SetShowDimension(false);
toolingBoxBuilder1->SetSingleOffset(false);
double effectiveOffsets[6] = {
    g_customBaoLonTiOffsets[0], g_customBaoLonTiOffsets[1],
    g_customBaoLonTiOffsets[2], g_customBaoLonTiOffsets[3],
    g_customBaoLonTiOffsets[4], g_customBaoLonTiOffsets[5]
};
if (g_customBaoLonTiUseGuard)
{
    for (int i = 0; i < 6; ++i)
    {
        if (effectiveOffsets[i] == 0.0)
        {
            effectiveOffsets[i] = 0.001;
        }
    }
}
std::string offsetPositiveX = std::to_string(effectiveOffsets[0]);
std::string offsetNegativeX = std::to_string(effectiveOffsets[1]);
std::string offsetPositiveY = std::to_string(effectiveOffsets[2]);
std::string offsetNegativeY = std::to_string(effectiveOffsets[3]);
std::string offsetPositiveZ = std::to_string(effectiveOffsets[4]);
std::string offsetNegativeZ = std::to_string(effectiveOffsets[5]);
toolingBoxBuilder1->OffsetPositiveX()->SetFormula(offsetPositiveX.c_str());
toolingBoxBuilder1->OffsetNegativeX()->SetFormula(offsetNegativeX.c_str());
toolingBoxBuilder1->OffsetPositiveY()->SetFormula(offsetPositiveY.c_str());
toolingBoxBuilder1->OffsetNegativeY()->SetFormula(offsetNegativeY.c_str());
toolingBoxBuilder1->OffsetPositiveZ()->SetFormula(offsetPositiveZ.c_str());
toolingBoxBuilder1->OffsetNegativeZ()->SetFormula(offsetNegativeZ.c_str());
toolingBoxBuilder1->SetBoxMatrixAndPosition(matrix1, workPart->WCS()->Origin());
NXOpen::SelectionIntentRuleOptions* selectionIntentRuleOptions1;
selectionIntentRuleOptions1 = workPart->ScRuleFactory()->CreateRuleOptions();
selectionIntentRuleOptions1->SetSelectedFromInactive(false);

NXOpen::EdgeDumbRule* edgeDumbRule1;
NXOpen::FaceDumbRule* FaceDumbRule1;
NXOpen::CurveDumbRule* curveDumbRule1;
std::vector<NXOpen::SelectionIntentRule*> rules1(3);
if (VEdge1.size() > 0)
{
    edgeDumbRule1 = workPart->ScRuleFactory()->CreateRuleEdgeDumb(VEdge1, selectionIntentRuleOptions1);
    rules1[0] = edgeDumbRule1;
}
if (VFace1.size() > 0)
{
    FaceDumbRule1 = workPart->ScRuleFactory()->CreateRuleFaceDumb(VFace1, selectionIntentRuleOptions1);
    rules1[1] = FaceDumbRule1;
}
if (VPoint1.size() > 0)
{
    curveDumbRule1 = workPart->ScRuleFactory()->CreateRuleCurveDumbFromPoints(VPoint1, selectionIntentRuleOptions1);
    rules1[2] = curveDumbRule1;
}
delete selectionIntentRuleOptions1;
NXOpen::ScCollector* scCollector1;
scCollector1 = toolingBoxBuilder1->BoundedObject();
scCollector1->ReplaceRules(rules1, false);

std::vector<NXOpen::NXObject*> selectedOccurrences;
selectedOccurrences.reserve(VTaggedObject1.size());
for (size_t i = 0; i < VTaggedObject1.size(); ++i)
{
    NXOpen::NXObject* selectedObject = dynamic_cast<NXOpen::NXObject*>(VTaggedObject1[i]);
    if (selectedObject != NULL)
    {
        selectedOccurrences.push_back(selectedObject);
    }
}
std::vector<NXOpen::NXObject*> deselectedOccurrences(0);
toolingBoxBuilder1->SetSelectedOccurrences(selectedOccurrences, deselectedOccurrences);

NXOpen::SelectNXObjectList* facetBodies1;
facetBodies1 = toolingBoxBuilder1->FacetBodies();
std::vector<NXOpen::NXObject*> facetObjects1(0);
facetBodies1->Add(facetObjects1);

toolingBoxBuilder1->CalculateBoxSize();
toolingBoxBuilder1->PreviewBuilder()->Preview();
NXOpen::NXObject* nXObject1;
nXObject1 = toolingBoxBuilder1->Commit();
toolingBox1 = dynamic_cast<NXOpen::Features::ToolingBox*>(NXOpen::NXObjectManager::Get(nXObject1->Tag()));
Body2 = toolingBox1->GetBodies()[0];
theSession->Preferences()->VisualizationVisualPreferences()->SetTranslucency(true);

NXOpen::DisplayModification* displayModification1;
displayModification1 = theSession->DisplayManager()->NewDisplayModification();
displayModification1->SetApplyToAllFaces(true);
displayModification1->SetApplyToOwningParts(false);
displayModification1->SetNewWidth(NXOpen::DisplayableObject::ObjectWidthOne);
displayModification1->SetNewTranslucency(90);
std::vector<NXOpen::DisplayableObject*> objects11(1);
objects11[0] = Body2;
displayModification1->Apply(objects11);
delete displayModification1;
toolingBoxBuilder1->Destroy();
}


//删除对象
void custom_del(vector<NXOpen::TaggedObject*> objects1)//输入要删除的对象
{
    int nErrs1;
    nErrs1 = theSession->UpdateManager()->AddObjectsToDeleteList(objects1);
    NXOpen::Session::UndoMarkId id1;
    id1 = theSession->NewestVisibleUndoMark();
    int nErrs2;
    nErrs2 = theSession->UpdateManager()->DoUpdate(id1);
 }


//输入指定方位获得Matrix3x3
extern void custom_manip_getMatrix(NXOpen::BlockStyler::SpecifyOrientation* manip0, NXOpen::Matrix3x3 &matrix1)//输入指定方位
{
    double X_vec[3];
    double Y_vec[3];
    double mtx[9];
    X_vec[0] = manip0->XAxis().X;
    X_vec[1] = manip0->XAxis().Y;
    X_vec[2] = manip0->XAxis().Z;

    Y_vec[0] = manip0->YAxis().X;
    Y_vec[1] = manip0->YAxis().Y;
    Y_vec[2] = manip0->YAxis().Z;

    UF_MTX3_initialize(X_vec, Y_vec, mtx);
    matrix1.Xx = mtx[0]; matrix1.Xy = mtx[1]; matrix1.Xz = mtx[2];
    matrix1.Yx = mtx[3]; matrix1.Yy = mtx[4]; matrix1.Yz = mtx[5];
    matrix1.Zx = mtx[6]; matrix1.Zy = mtx[7]; matrix1.Zz = mtx[8];
}

//创建方体
void custom_box(NXOpen::Point3d Point3d2, NXOpen::Matrix3x3& matrix1, const char* x, const char* y, const char* z, Features::Feature*& Feature1)
{
    NXOpen::Features::ToolingBox* nullNXOpen_Features_ToolingBox(NULL);
    NXOpen::Features::ToolingBoxBuilder* toolingBoxBuilder1;
    toolingBoxBuilder1 = workPart->Features()->ToolingFeatureCollection()->CreateToolingBoxBuilder(nullNXOpen_Features_ToolingBox);
    toolingBoxBuilder1->SetType(NXOpen::Features::ToolingBoxBuilder::TypesCenterAndLengths); 
    toolingBoxBuilder1->SetBoxMatrixAndPosition(matrix1, Point3d2);
    toolingBoxBuilder1->XValue()->SetFormula(x);
    toolingBoxBuilder1->YValue()->SetFormula(y);
    toolingBoxBuilder1->ZValue()->SetFormula(z);
    toolingBoxBuilder1->CalculateBoxSize();
    Feature1 = toolingBoxBuilder1->CommitFeature();
    toolingBoxBuilder1->Destroy();
    theSession->CleanUpFacetedFacesAndEdges();
}
//修改方体
void custom_rebox(NXOpen::Point3d Point3d2, NXOpen::Matrix3x3& matrix1, const char* x, const char* y, const char* z, Features::Feature* &Feature1)
{
    NXOpen::Session::UndoMarkId markId1;
    markId1 = theSession->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "Redefine Feature");
    NXOpen::Features::ToolingBox* toolingBox1(dynamic_cast<NXOpen::Features::ToolingBox*>(Feature1));
    NXOpen::Features::EditWithRollbackManager* editWithRollbackManager1;
    editWithRollbackManager1 = workPart->Features()->StartEditWithRollbackManager(toolingBox1, markId1);

    NXOpen::Features::ToolingBoxBuilder* toolingBoxBuilder1;
    toolingBoxBuilder1 = workPart->Features()->ToolingFeatureCollection()->CreateToolingBoxBuilder(toolingBox1);

    toolingBoxBuilder1->SetReferenceCsysType(NXOpen::Features::ToolingBoxBuilder::RefCsysTypeAbsoluteinDisplayedPart);

    toolingBoxBuilder1->SetBoxMatrixAndPosition(matrix1, Point3d2);
    toolingBoxBuilder1->XValue()->SetFormula(x);
    toolingBoxBuilder1->YValue()->SetFormula(y);
    toolingBoxBuilder1->ZValue()->SetFormula(z);
    toolingBoxBuilder1->CalculateBoxSize();

    NXOpen::NXObject* nXObject1;
    nXObject1 = toolingBoxBuilder1->Commit();
    toolingBoxBuilder1->Destroy();

    editWithRollbackManager1->UpdateFeature(false);

    editWithRollbackManager1->Stop();

    editWithRollbackManager1->Destroy();
}


