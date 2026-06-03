#include "ZiDonCuTuCurve.hpp"

std::vector<NXOpen::Drawings::DraftingBody*> CollectDraftingBodies(NXOpen::Drawings::BaseView* baseView)
{
	std::vector<NXOpen::Drawings::DraftingBody*> bodies;
	if (baseView == NULL)
	{
		return bodies;
	}

	NXOpen::Drawings::DraftingBodyCollection* bodyCollection = baseView->DraftingBodies();
	for (NXOpen::Drawings::DraftingBodyCollection::iterator it = bodyCollection->begin(); it != bodyCollection->end(); ++it)
	{
		bodies.push_back(*it);
	}
	return bodies;
}

std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::DraftingBody* draftingBody)
{
	std::vector<NXOpen::Drawings::DraftingCurve*> curves;
	if (draftingBody == NULL)
	{
		return curves;
	}

	NXOpen::Drawings::DraftingCurveCollection* curveCollection = draftingBody->DraftingCurves();
	for (NXOpen::Drawings::DraftingCurveCollection::iterator it = curveCollection->begin(); it != curveCollection->end(); ++it)
	{
		curves.push_back(*it);
	}
	return curves;
}

std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::BaseView* baseView)
{
	std::vector<NXOpen::Drawings::DraftingCurve*> curves;
	const std::vector<NXOpen::Drawings::DraftingBody*> bodies = CollectDraftingBodies(baseView);
	for (size_t i = 0; i < bodies.size(); ++i)
	{
		std::vector<NXOpen::Drawings::DraftingCurve*> bodyCurves = CollectDraftingCurves(bodies[i]);
		curves.insert(curves.end(), bodyCurves.begin(), bodyCurves.end());
	}
	return curves;
}

NXOpen::Drawings::DraftingBody* FindDraftingBodyForModelBody(NXOpen::Drawings::BaseView* baseView, NXOpen::Body* modelBody)
{
	if (baseView == NULL || modelBody == NULL)
	{
		return NULL;
	}

	const std::vector<NXOpen::Drawings::DraftingBody*> bodies = CollectDraftingBodies(baseView);
	for (size_t i = 0; i < bodies.size(); ++i)
	{
		const std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(bodies[i]);
		if (curves.empty())
		{
			continue;
		}

		const std::vector<NXOpen::NXObject*> parents = curves[0]->GetDraftingCurveInfo()->GetParents();
		if (parents.empty() || parents[0] == NULL)
		{
			continue;
		}

		int type = 0;
		int subtype = 0;
		UF_OBJ_ask_type_and_subtype(parents[0]->Tag(), &type, &subtype);
		if (subtype == UF_solid_face_subtype)
		{
			NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(parents[0]->Tag()));
			if (face != NULL && modelBody == face->GetBody())
			{
				return bodies[i];
			}
		}
		else if (subtype == UF_solid_edge_subtype)
		{
			NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(NXOpen::NXObjectManager::Get(parents[0]->Tag()));
			if (edge != NULL && modelBody == edge->GetBody())
			{
				return bodies[i];
			}
		}
	}

	return NULL;
}
