//	CTopologyNode.cpp
//
//	Star system topology

#include "PreComp.h"

#define ATTRIBUTES_TAG							CONSTLIT("Attributes")
#define CHANCE_TAG								CONSTLIT("Chance")
#define DISTANCE_BETWEEN_NODES_TAG				CONSTLIT("DistanceBetweenNodes")
#define DISTANCE_TO_TAG							CONSTLIT("DistanceTo")
#define STARGATE_COUNT_TAG						CONSTLIT("StargateCount")
#define SYSTEM_TAG								CONSTLIT("System")
#define STARGATES_TAG							CONSTLIT("StarGates")

#define ATTRIBUTES_ATTRIB						CONSTLIT("attributes")
#define CHANCE_ATTRIB							CONSTLIT("chance")
#define CRITERIA_ATTRIB							CONSTLIT("criteria")
#define DEST_FRAGMENT_ROTATION_ATTRIB			CONSTLIT("destFragmentRotation")
#define DESTGATE_ATTRIB							CONSTLIT("destGate")
#define DESTID_ATTRIB							CONSTLIT("destID")
#define EPITAPH_ATTRIB							CONSTLIT("epitaph")
#define END_GAME_ATTRIB							CONSTLIT("endGame")
#define END_GAME_REASON_ATTRIB					CONSTLIT("endGameReason")
#define ID_ATTRIB								CONSTLIT("ID")
#define LEVEL_ATTRIB							CONSTLIT("level")
#define MAX_ATTRIB								CONSTLIT("max")
#define MIN_ATTRIB								CONSTLIT("min")
#define NAME_ATTRIB								CONSTLIT("name")
#define NODE_ID_ATTRIB							CONSTLIT("nodeID")
#define ROOT_NODE_ATTRIB						CONSTLIT("rootNode")
#define UNID_ATTRIB								CONSTLIT("UNID")
#define VARIANT_ATTRIB							CONSTLIT("variant")
#define X_ATTRIB								CONSTLIT("x")
#define Y_ATTRIB								CONSTLIT("y")

#define PREV_DEST								CONSTLIT("[Prev]")

#define PROPERTY_LEVEL							CONSTLIT("level")
#define PROPERTY_NAME							CONSTLIT("name")
#define PROPERTY_POS							CONSTLIT("pos")

#define SPECIAL_NODE_ID							CONSTLIT("nodeID:")

//	CTopologyNode class --------------------------------------------------------

CTopologyNode::CTopologyNode (const CString &sID, DWORD SystemUNID, CSystemMap *pMap) : m_sID(sID),
		m_SystemUNID(SystemUNID),
		m_pMap(pMap),
		m_NamedGates(FALSE, TRUE),
		m_pSystem(NULL),
		m_dwID(0xffffffff),
		m_bKnown(false)

//	CTopology constructor

	{
#ifdef DEBUG_ALL_NODES
	m_bKnown = true;
#endif
	}

CTopologyNode::~CTopologyNode (void)

//	CTopology destructor

	{
	for (int i = 0; i < m_NamedGates.GetCount(); i++)
		{
		StarGateDesc *pDesc = (StarGateDesc *)m_NamedGates.GetValue(i);
		delete pDesc;
		}
	}

void CTopologyNode::AddAttributes (const CString &sAttribs)

//	AddAttributes
//
//	Append the given attributes

	{
	m_sAttributes = ::AppendModifiers(m_sAttributes, sAttribs);
	}

ALERROR CTopologyNode::AddGateInt (const CString &sName, const CString &sDestNode, const CString &sEntryPoint)

//	AddGateInt
//
//	Adds a gate to the topology

	{
	ALERROR error;

	StarGateDesc *pDesc = new StarGateDesc;
	pDesc->sDestNode = sDestNode;
	pDesc->sDestEntryPoint = sEntryPoint;
	pDesc->pDestNode = NULL;

	//	Note: If we already have a gate of that name, this will (appropriately) fail.

	if (error = m_NamedGates.AddEntry(sName, (CObject *)pDesc))
		{
		delete pDesc;
		return error;
		}

	return NOERROR;
	}

ALERROR CTopologyNode::AddStargate (const CString &sGateID, const CString &sDestNodeID, const CString &sDestGateID)

//	AddStargate
//
//	Adds a new stargate to the topology

	{
	//	Get the destination node

	CTopologyNode *pDestNode = g_pUniverse->FindTopologyNode(sDestNodeID);
	if (pDestNode == NULL)
		{
		kernelDebugLogMessage("Unable to find destination node: %s", sDestNodeID);
		return ERR_FAIL;
		}

	//	Look for the destination stargate

	CString sReturnNodeID;
	CString sReturnEntryPoint;
	if (!pDestNode->FindStargate(sDestGateID, &sReturnNodeID, &sReturnEntryPoint))
		{
		kernelDebugLogMessage("Unable to find destination stargate: %s", sDestGateID);
		return ERR_FAIL;
		}

	//	Add the gate

	AddGateInt(sGateID, sDestNodeID, sDestGateID);

	//	See if we need to fix up the return gate

	if (strEquals(sReturnNodeID, PREV_DEST))
		pDestNode->SetStargateDest(sDestGateID, GetID(), sGateID);

	return NOERROR;
	}

ALERROR CTopologyNode::AddStargateConnection (CTopologyNode *pDestNode, bool bOneWay, const CString &sFromName, const CString &sToName)

//	AddStargateConnection
//
//	Adds two stargates connecting this node with the destination.
//	The names of the stargates are optionally passed in (if not, they are autogenerated).

	{
	ALERROR error;

	CString sSourceGate = (sFromName.IsBlank() ? GenerateStargateName() : sFromName);
	CString sDestGate = ((!pDestNode->IsEndGame() && sToName.IsBlank()) ? pDestNode->GenerateStargateName() : sToName);

	//	Add stargate from source to destination

	if (error = AddGateInt(sSourceGate, pDestNode->GetID(), sDestGate))
		return ERR_FAIL;

	//	Check to see if the destination gate exists already
	//	[If sToName is blank, then we know that the gate does not exist because we autogenerated it.]

	bool bExists = (!sToName.IsBlank() && (pDestNode->m_NamedGates.Lookup(sDestGate) == NOERROR));

	//	If the gate doesn't exist AND we want both directions, create the destination gate
	//	[We don't create the other gate if it already exists OR we want a one-way gate.

	if (!bExists && !bOneWay && !pDestNode->IsEndGame())
		{
		if (error = pDestNode->AddGateInt(sDestGate, GetID(), sSourceGate))
			return ERR_FAIL;
		}

	//	Done

	return NOERROR;
	}

int CTopologyNode::CalcMatchStrength (const CAttributeCriteria &Criteria)

//	CalcMatchStrength
//
//	Calculates the match strength of topology node and the criteria.

	{
	int i;

	int iStrength = 1000;
	for (i = 0; i < Criteria.GetCount(); i++)
		{
		DWORD dwMatchStrength;
		bool bIsSpecial;
		const CString &sAttrib = Criteria.GetAttribAndWeight(i, &dwMatchStrength, &bIsSpecial);

		bool bHasAttrib = (bIsSpecial ? HasSpecialAttribute(sAttrib) : HasAttribute(sAttrib));
		int iAdj = CAttributeCriteria::CalcWeightAdj(bHasAttrib, dwMatchStrength);

		iStrength = iStrength * iAdj / 1000;
		}

	return iStrength;
	}

void CTopologyNode::CreateFromStream (SUniverseLoadCtx &Ctx, CTopologyNode **retpNode)

//	CreateFromStream
//
//	Creates a node from a stream
//
//	CString		m_sID
//	DWORD		m_SystemUNID
//	DWORD		m_pMap (UNID)
//	DWORD		m_xPos
//	DWORD		m_yPos
//	CString		m_sName
//	CString		m_sAttributes
//	DWORD		m_iLevel
//	DWORD		m_dwID
//
//	DWORD		No of named gates
//	CString		gate: sName
//	CString		gate: sDestNode
//	CString		gate: sDestEntryPoint
//
//	DWORD		No of variant labels
//	CString		variant label
//
//	CAttributeDataBlock	m_Data
//	DWORD		flags
//
//	CString		m_sEpitaph
//	CString		m_sEndGameReason

	{
	int i;
	DWORD dwLoad;
	CTopologyNode *pNode;

	CString sID;
	sID.ReadFromStream(Ctx.pStream);

	DWORD dwSystemUNID;
	Ctx.pStream->Read((char *)&dwSystemUNID, sizeof(DWORD));

	CSystemMap *pMap;
	if (Ctx.dwVersion >= 6)
		{
		DWORD dwMapUNID;
		Ctx.pStream->Read((char *)&dwMapUNID, sizeof(DWORD));
		pMap = CSystemMap::AsType(g_pUniverse->FindDesignType(dwMapUNID));
		}
	else
		pMap = NULL;

	pNode = new CTopologyNode(sID, dwSystemUNID, pMap);

	if (Ctx.dwVersion >= 6)
		{
		Ctx.pStream->Read((char *)&pNode->m_xPos, sizeof(DWORD));
		Ctx.pStream->Read((char *)&pNode->m_yPos, sizeof(DWORD));
		}
	
	pNode->m_sName.ReadFromStream(Ctx.pStream);
	if (Ctx.dwVersion >= 23)
		pNode->m_sAttributes.ReadFromStream(Ctx.pStream);

	Ctx.pStream->Read((char *)&pNode->m_iLevel, sizeof(DWORD));
	Ctx.pStream->Read((char *)&pNode->m_dwID, sizeof(DWORD));

	DWORD dwCount;
	Ctx.pStream->Read((char *)&dwCount, sizeof(DWORD));
	for (i = 0; i < (int)dwCount; i++)
		{
		StarGateDesc *pDesc = new StarGateDesc;
		CString sName;
		sName.ReadFromStream(Ctx.pStream);
		pDesc->sDestNode.ReadFromStream(Ctx.pStream);
		pDesc->sDestEntryPoint.ReadFromStream(Ctx.pStream);
		pDesc->pDestNode = NULL;

		pNode->m_NamedGates.AddEntry(sName, (CObject *)pDesc);
		}

	Ctx.pStream->Read((char *)&dwCount, sizeof(DWORD));
	for (i = 0; i < (int)dwCount; i++)
		{
		CString sLabel;
		sLabel.ReadFromStream(Ctx.pStream);
		pNode->m_VariantLabels.Insert(sLabel);
		}

	if (Ctx.dwVersion >= 1)
		pNode->m_Data.ReadFromStream(Ctx.pStream);

	//	Flags

	if (Ctx.dwVersion >= 6)
		Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	else
		dwLoad = 0;

	pNode->m_bKnown = (dwLoad & 0x00000001 ? true : false);
	pNode->m_bMarked = false;

	//	More

	if (Ctx.dwVersion >= 5)
		{
		pNode->m_sEpitaph.ReadFromStream(Ctx.pStream);
		pNode->m_sEndGameReason.ReadFromStream(Ctx.pStream);
		}
	else
		{
		//	For previous version, we forgot to save this, so do it now

		if (pNode->IsEndGame())
			{
			pNode->m_sEpitaph = CONSTLIT("left Human Space on a journey to the Galactic Core");
			pNode->m_sEndGameReason = CONSTLIT("leftHumanSpace");
			}
		}

	//	Done

	*retpNode = pNode;
	}

bool CTopologyNode::FindStargate (const CString &sName, CString *retsDestNode, CString *retsEntryPoint)

//	FindStargate
//
//	Looks for the stargate by name and returns the destination node id and entry point

	{
	StarGateDesc *pDesc;
	if (m_NamedGates.Lookup(sName, (CObject **)&pDesc) != NOERROR)
		return false;

	if (retsDestNode)
		*retsDestNode = pDesc->sDestNode;

	if (retsEntryPoint)
		*retsEntryPoint = pDesc->sDestEntryPoint;

	return true;
	}

CString CTopologyNode::FindStargateName (const CString &sDestNode, const CString &sEntryPoint)

//	FindStargateName
//
//	Returns the name of the stargate that matches the node and entry point

	{
	int i;

	for (i = 0; i < m_NamedGates.GetCount(); i++)
		{
		StarGateDesc *pDesc = (StarGateDesc *)m_NamedGates.GetValue(i);
		if (strEquals(pDesc->sDestNode, sDestNode)
				&& strEquals(pDesc->sDestEntryPoint, sEntryPoint))
			return m_NamedGates.GetKey(i);
		}

	return NULL_STR;
	}

bool CTopologyNode::FindStargateTo (const CString &sDestNode, CString *retsName, CString *retsDestGateID)

//	FindStargateTo
//
//	Looks for a stargate to the given node; returns info on the first one.
//	Returns FALSE if none found.

	{
	int i;

	for (i = 0; i < m_NamedGates.GetCount(); i++)
		{
		StarGateDesc *pDesc = (StarGateDesc *)m_NamedGates.GetValue(i);
		if (strEquals(pDesc->sDestNode, sDestNode))
			{
			if (retsName)
				*retsName = m_NamedGates.GetKey(i);

			if (retsDestGateID)
				*retsDestGateID = pDesc->sDestEntryPoint;

			return true;
			}
		}

	return false;
	}

CString CTopologyNode::GenerateStargateName (void)

//	GenerateStargateName
//
//	Generates a unique stargate name

	{
	CString sName;
	int iIndex = m_NamedGates.GetCount();

	do
		{
		iIndex++;
		sName = strPatternSubst(CONSTLIT("SG%d"), iIndex);
		}
	while (m_NamedGates.Lookup(sName) == NOERROR);

	//	Done

	return sName;
	}

CTopologyNode *CTopologyNode::GetGateDest (const CString &sName, CString *retsEntryPoint)

//	GetGateDest
//
//	Get stargate destination

	{
	StarGateDesc *pDesc;
	if (m_NamedGates.Lookup(sName, (CObject **)&pDesc) != NOERROR)
		return NULL;

	if (retsEntryPoint)
		*retsEntryPoint = pDesc->sDestEntryPoint;

	if (pDesc->pDestNode == NULL)
		pDesc->pDestNode = g_pUniverse->FindTopologyNode(pDesc->sDestNode);

	return pDesc->pDestNode;
	}

ICCItem *CTopologyNode::GetProperty (const CString &sName)

//	GetProperty
//
//	Get topology node property

	{
	CCodeChain &CC = g_pUniverse->GetCC();

	if (strEquals(sName, PROPERTY_LEVEL))
		return CC.CreateInteger(GetLevel());
	else if (strEquals(sName, PROPERTY_NAME))
		return CC.CreateString(GetSystemName());
	else if (strEquals(sName, PROPERTY_POS))
		{
		//	If no map, then no position

		if (m_pMap == NULL)
			return CC.CreateNil();

		//	Create a list

		ICCItem *pResult = CC.CreateLinkedList();
		if (pResult->IsError())
			return pResult;

		CCLinkedList *pList = (CCLinkedList *)pResult;

		pList->AppendInteger(CC, m_xPos);
		pList->AppendInteger(CC, m_yPos);

		return pResult;
		}
	else
		return CC.CreateNil();
	}

CString CTopologyNode::GetStargate (int iIndex)

//	GetStargate
//
//	Returns the stargate ID

	{
	return m_NamedGates.GetKey(iIndex);
	}

CTopologyNode *CTopologyNode::GetStargateDest (int iIndex, CString *retsEntryPoint)

//	GetStargateDest
//
//	Returns the destination node for the given stargate

	{
	StarGateDesc *pDesc = (StarGateDesc *)m_NamedGates.GetValue(iIndex);
	if (retsEntryPoint)
		*retsEntryPoint = pDesc->sDestEntryPoint;

	if (pDesc->pDestNode == NULL)
		pDesc->pDestNode = g_pUniverse->FindTopologyNode(pDesc->sDestNode);

	return pDesc->pDestNode;
	}

bool CTopologyNode::HasSpecialAttribute (const CString &sAttrib) const

//	HasSpecialAttribute
//
//	Returns TRUE if we have the special attribute

	{
	if (strStartsWith(sAttrib, SPECIAL_NODE_ID))
		{
		CString sNodeID = strSubString(sAttrib, SPECIAL_NODE_ID.GetLength());
		return strEquals(sNodeID, GetID());
		}
	else
		return false;
	}

bool CTopologyNode::HasVariantLabel (const CString &sVariant)

//	HasVariantLabel
//
//	Returns TRUE if it has the given variant label

	{
	for (int i = 0; i < m_VariantLabels.GetCount(); i++)
		{
		if (strEquals(sVariant, m_VariantLabels[i]))
			return true;
		}

	return false;
	}

ALERROR CTopologyNode::InitFromAdditionalXML (CXMLElement *pDesc, CString *retsError)

//	InitFromAdditionalXML
//
//	Adds additional information

	{
	ALERROR error;

	if (strEquals(pDesc->GetTag(), SYSTEM_TAG))
		{
		if (error = InitFromSystemXML(pDesc, retsError))
			return error;
		}
	else if (strEquals(pDesc->GetTag(), ATTRIBUTES_TAG))
		{
		if (error = InitFromAttributesXML(pDesc, retsError))
			return error;
		}

	return NOERROR;
	}

ALERROR CTopologyNode::InitFromAttributesXML (CXMLElement *pAttributes, CString *retsError)

//	InitFromAttributesXML
//
//	Adds attributes

	{
	AddAttributes(pAttributes->GetAttribute(ATTRIBUTES_ATTRIB));

	return NOERROR;
	}

ALERROR CTopologyNode::InitFromSystemXML (CXMLElement *pSystem, CString *retsError)

//	InitFromSystemXML
//
//	Initializes the system information based on an XML element.
//	NOTE: We assume the universe is fully bound at this point.

	{
	ALERROR error;
	CString sSystemUNID = pSystem->GetAttribute(UNID_ATTRIB);
	DWORD dwUNID = strToInt(sSystemUNID, 0, NULL);

	//	If the system node contains a table of different system types, then
	//	remember the root node because some of the system information (such as the
	//	name) may be there.

	CXMLElement *pSystemParent = NULL;

	//	If there is no UNID attribute then it means that the system
	//	is randomly determined based on a table

	if (dwUNID == 0 && pSystem->GetContentElementCount() == 1)
		{
		CXMLElement *pTableElement = pSystem->GetContentElement(0);
		if (pTableElement == NULL)
			{
			ASSERT(false);
			return ERR_FAIL;
			}

		CRandomEntryResults System;
		if (error = CRandomEntryGenerator::Generate(pTableElement, System))
			{
			*retsError = strPatternSubst(CONSTLIT("Topology %s: Unable to generate random system UNID"), m_sID);
			return ERR_FAIL;
			}

		if (System.GetCount() != 1)
			{
			*retsError = strPatternSubst(CONSTLIT("Topology %s: Table generated no systems"), m_sID);
			return ERR_FAIL;
			}

		pSystemParent = pSystem;
		pSystem = System.GetResult(0);
		dwUNID = pSystem->GetAttributeInteger(UNID_ATTRIB);
		}

	//	Set the system UNID

	if (dwUNID != 0)
		m_SystemUNID = dwUNID;

	//	Get the system type

	CSystemType *pSystemType = g_pUniverse->FindSystemType(m_SystemUNID);

	//	Set the name of the system

	CString sName;
	if (!pSystem->FindAttribute(NAME_ATTRIB, &sName))
		if (pSystemParent)
			sName = pSystemParent->GetAttribute(NAME_ATTRIB);

	if (!sName.IsBlank())
		SetName(sName);

	//	Set the level

	int iLevel = 0;
	if (!pSystem->FindAttributeInteger(LEVEL_ATTRIB, &iLevel))
		if (pSystemParent)
			iLevel = pSystemParent->GetAttributeInteger(LEVEL_ATTRIB);

	if (iLevel > 0)
		SetLevel(iLevel);

	if (GetLevel() == 0)
		SetLevel(1);

	//	Add variants for the system

	CString sVariant;
	if (pSystem->FindAttribute(VARIANT_ATTRIB, &sVariant))
		AddVariantLabel(sVariant);

	if (pSystemParent && pSystemParent->FindAttribute(VARIANT_ATTRIB, &sVariant))
		AddVariantLabel(sVariant);

	//	Add attributes for the node/system

	CString sAttribs;
	if (pSystem->FindAttribute(ATTRIBUTES_ATTRIB, &sAttribs))
		AddAttributes(sAttribs);

	if (pSystemParent && pSystemParent->FindAttribute(ATTRIBUTES_ATTRIB, &sAttribs))
		AddAttributes(sAttribs);

	if (pSystemType && !pSystemType->GetAttributes().IsBlank())
		AddAttributes(pSystemType->GetAttributes());

	return NOERROR;
	}

bool CTopologyNode::IsCriteriaAll (const SCriteria &Crit)

//	IsCriteriaAll
//
//	Returns TRUE if the criteria matches all nodes

	{
	return (Crit.iChance == 100
			&& Crit.iMaxInterNodeDist == -1
			&& Crit.iMinInterNodeDist == 0
			&& Crit.iMaxStargates == -1
			&& Crit.iMinStargates == 0
			&& Crit.AttribsNotAllowed.GetCount() == 0
			&& Crit.AttribsRequired.GetCount() == 0
			&& Crit.DistanceTo.GetCount() == 0
			&& Crit.SpecialRequired.GetCount() == 0
			&& Crit.SpecialNotAllowed.GetCount() == 0);
	}

bool CTopologyNode::MatchesCriteria (SCriteriaCtx &Ctx, const SCriteria &Crit)

//	MatchesCriteria
//
//	Returns TRUE if this node matches the given criteria

	{
	int i;

	//	Chance

	if (Crit.iChance < 100 && mathRandom(1, 100) > Crit.iChance)
		return false;

	//	Check required attributes

	for (i = 0; i < Crit.AttribsRequired.GetCount(); i++)
		if (!::HasModifier(m_sAttributes, Crit.AttribsRequired[i]))
			return false;

	//	Check disallowed attributes

	for (i = 0; i < Crit.AttribsNotAllowed.GetCount(); i++)
		if (::HasModifier(m_sAttributes, Crit.AttribsNotAllowed[i]))
			return false;

	//	Check special required attributes

	for (i = 0; i < Crit.SpecialRequired.GetCount(); i++)
		if (!HasSpecialAttribute(Crit.SpecialRequired[i]))
			return false;

	//	Check disallowed special attributes

	for (i = 0; i < Crit.SpecialNotAllowed.GetCount(); i++)
		if (HasSpecialAttribute(Crit.SpecialNotAllowed[i]))
			return false;

	//	Stargates

	if (m_NamedGates.GetCount() < Crit.iMinStargates)
		return false;

	if (Crit.iMaxStargates != -1 && m_NamedGates.GetCount() > Crit.iMaxStargates)
		return false;

	//	Distance to other nodes

	if (Ctx.pTopology)
		{
		for (i = 0; i < Crit.DistanceTo.GetCount(); i++)
			{
			//	If we don't have a specified nodeID then we need to find the distance
			//	to any node with the appropriate attributes

			if (Crit.DistanceTo[i].sNodeID.IsBlank())
				{
				CTopologyNodeList Checked;
				if (!Ctx.pTopology->GetTopologyNodeList().IsNodeInRangeOf(this,
						Crit.DistanceTo[i].iMinDist,
						Crit.DistanceTo[i].iMaxDist,
						Crit.DistanceTo[i].AttribsRequired,
						Crit.DistanceTo[i].AttribsNotAllowed,
						Checked))
					return false;
				}

			//	Otherwise, find the distance to the given node

			else
				{
				int iDist = Ctx.pTopology->GetDistance(GetID(), Crit.DistanceTo[i].sNodeID);

				if (iDist != -1 && iDist < Crit.DistanceTo[i].iMinDist)
					return false;

				if (iDist == -1 || (Crit.DistanceTo[i].iMaxDist != -1 && iDist > Crit.DistanceTo[i].iMaxDist))
					return false;
				}
			}
		}

	//	Done

	return true;
	}

ALERROR CTopologyNode::ParseCriteria (const CString &sCriteria, SCriteria *retCrit, CString *retsError)

//	ParseCriteria
//
//	Parses a string criteria

	{
	retCrit->iChance = 100;
	retCrit->iMaxInterNodeDist = -1;
	retCrit->iMinInterNodeDist = 0;
	retCrit->iMaxStargates = -1;
	retCrit->iMinStargates = 0;

	return ParseCriteriaInt(sCriteria, retCrit);
	}

ALERROR CTopologyNode::ParseCriteria (CXMLElement *pCrit, SCriteria *retCrit, CString *retsError)

//	ParseCriteria
//
//	Parses an XML element into a criteria desc

	{
	int i;

	retCrit->iChance = 100;
	retCrit->iMaxInterNodeDist = -1;
	retCrit->iMinInterNodeDist = 0;
	retCrit->iMaxStargates = -1;
	retCrit->iMinStargates = 0;

	if (pCrit)
		{
		for (i = 0; i < pCrit->GetContentElementCount(); i++)
			{
			CXMLElement *pItem = pCrit->GetContentElement(i);

			if (strEquals(pItem->GetTag(), ATTRIBUTES_TAG))
				{
				CString sCriteria = pItem->GetAttribute(CRITERIA_ATTRIB);
				ParseCriteriaInt(sCriteria, retCrit);
				}
			else if (strEquals(pItem->GetTag(), CHANCE_TAG))
				{
				retCrit->iChance = pItem->GetAttributeIntegerBounded(CHANCE_ATTRIB, 0, 100, 100);
				}
			else if (strEquals(pItem->GetTag(), DISTANCE_BETWEEN_NODES_TAG))
				{
				retCrit->iMinInterNodeDist = pItem->GetAttributeIntegerBounded(MIN_ATTRIB, 0, -1, 0);
				retCrit->iMaxInterNodeDist = pItem->GetAttributeIntegerBounded(MAX_ATTRIB, 0, -1, -1);
				}
			else if (strEquals(pItem->GetTag(), DISTANCE_TO_TAG))
				{
				SDistanceTo *pDistTo = retCrit->DistanceTo.Insert();
				pDistTo->iMinDist = pItem->GetAttributeIntegerBounded(MIN_ATTRIB, 0, -1, 0);
				pDistTo->iMaxDist = pItem->GetAttributeIntegerBounded(MAX_ATTRIB, 0, -1, -1);

				CString sCriteria;
				if (pItem->FindAttribute(CRITERIA_ATTRIB, &sCriteria))
					{
					SCriteria Criteria;
					if (ParseCriteriaInt(sCriteria, &Criteria) != NOERROR)
						{
						*retsError = strPatternSubst(CONSTLIT("Unable to parse criteria: %s"), sCriteria);
						return ERR_FAIL;
						}

					pDistTo->AttribsRequired = Criteria.AttribsRequired;
					pDistTo->AttribsNotAllowed = Criteria.AttribsNotAllowed;
					}
				else
					pDistTo->sNodeID = pItem->GetAttribute(NODE_ID_ATTRIB);
				}
			else if (strEquals(pItem->GetTag(), STARGATE_COUNT_TAG))
				{
				retCrit->iMinStargates = pItem->GetAttributeIntegerBounded(MIN_ATTRIB, 0, -1, 0);
				retCrit->iMaxStargates = pItem->GetAttributeIntegerBounded(MAX_ATTRIB, 0, -1, -1);
				}
			else
				{
				*retsError = strPatternSubst(CONSTLIT("Unknown criteria element: %s"), pItem->GetTag());
				return ERR_FAIL;
				}
			}
		}

	return NOERROR;
	}

ALERROR CTopologyNode::ParseCriteriaInt (const CString &sCriteria, SCriteria *retCrit)

//	ParseCriteriaInt
//
//	Parses a string criteria

	{
	char *pPos = sCriteria.GetASCIIZPointer();
	while (*pPos != '\0')
		{
		switch (*pPos)
			{
			case '+':
			case '-':
				{
				bool bRequired = (*pPos == '+');
				bool bBinaryParam;
				CString sParam = ::ParseCriteriaParam(&pPos, false, &bBinaryParam);

				if (bRequired)
					{
					if (bBinaryParam)
						retCrit->SpecialRequired.Insert(sParam);
					else
						retCrit->AttribsRequired.Insert(sParam);
					}
				else
					{
					if (bBinaryParam)
						retCrit->SpecialNotAllowed.Insert(sParam);
					else
						retCrit->AttribsNotAllowed.Insert(sParam);
					}
				break;
				}
			}

		pPos++;
		}

	return NOERROR;
	}

ALERROR CTopologyNode::ParsePosition (const CString &sValue, int *retx, int *rety)

//	ParsePosition
//
//	Parse a node position (x,y)

	{
	//	Pre-init

	*retx = 0;
	*rety = 0;

	//	Parse

	char *pPos = sValue.GetASCIIZPointer();

	bool bInvalid;
	*retx = ::strParseInt(pPos, 0, &pPos, &bInvalid);
	if (bInvalid)
		return ERR_FAIL;

	//	Skip whitespace

	while (::strIsWhitespace(pPos))
		pPos++;

	//	Skip delimeter

	if (*pPos != ',')
		return ERR_FAIL;
	pPos++;

	//	Next value

	*rety = ::strParseInt(pPos, 0, &pPos, &bInvalid);
	if (bInvalid)
		return ERR_FAIL;

	return NOERROR;
	}

ALERROR CTopologyNode::ParseStargateString (const CString &sStargate, CString *retsNodeID, CString *retsGateName)

//	ParseStargateString
//
//	Parses stargate from a single string ("nodeID:gateName")
//
//	Note: Callers rely on the fact that a NULL_STR input results in NULL_STR outputs (and NOERROR)

	{
	char *pPos = sStargate.GetASCIIZPointer();
	char *pStart = pPos;
	while (*pPos != ':' && *pPos != '\0')
		pPos++;

	*retsNodeID = CString(pStart, pPos - pStart);

	if (*pPos == ':')
		*retsGateName = CString(pPos + 1);
	else
		*retsGateName = NULL_STR;

	return NOERROR;
	}

bool CTopologyNode::SetProperty (const CString &sName, ICCItem *pValue, CString *retsError)

//	SetProperty
//
//	Set topology node property

	{
	CCodeChain &CC = g_pUniverse->GetCC();

	if (strEquals(sName, PROPERTY_POS))
		{
		if (m_pMap == NULL)
			{
			*retsError = CONSTLIT("Node is not on a system map and cannot be positioned.");
			return false;
			}

		if (pValue->GetCount() < 2)
			{
			*retsError = CONSTLIT("Invalid node coordinate.");
			return false;
			}

		m_xPos = pValue->GetElement(0)->GetIntegerValue();
		m_yPos = pValue->GetElement(1)->GetIntegerValue();

		return true;
		}
	else
		return false;
	}

void CTopologyNode::SetStargateDest (const CString &sName, const CString &sDestNode, const CString &sEntryPoint)

//	SetStargateDest
//
//	Sets the destination information for the given stargate

	{
	StarGateDesc *pDesc;
	if (m_NamedGates.Lookup(sName, (CObject **)&pDesc) != NOERROR)
		{
		ASSERT(false);
		return;
		}

	pDesc->sDestNode = sDestNode;
	pDesc->sDestEntryPoint = sEntryPoint;
	pDesc->pDestNode = NULL;
	}

void CTopologyNode::WriteToStream (IWriteStream *pStream)

//	WriteToStream
//
//	Writes out the variable portions of the node
//
//	CString		m_sID
//	DWORD		m_SystemUNID
//	DWORD		m_pMap (UNID)
//	DWORD		m_xPos
//	DWORD		m_yPos
//	CString		m_sName
//	CString		m_sAttributes
//	DWORD		m_iLevel
//	DWORD		m_dwID
//
//	DWORD		No of named gates
//	CString		gate: sName
//	CString		gate: sDestNode
//	CString		gate: sDestEntryPoint
//
//	DWORD		No of variant labels
//	CString		variant label
//
//	CAttributeDataBlock	m_Data
//	DWORD		flags
//
//	CString		m_sEpitaph
//	CString		m_sEndGameReason

	{
	int i;
	DWORD dwSave;

	m_sID.WriteToStream(pStream);
	pStream->Write((char *)&m_SystemUNID, sizeof(DWORD));

	dwSave = (m_pMap ? m_pMap->GetUNID() : 0);
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	pStream->Write((char *)&m_xPos, sizeof(DWORD));
	pStream->Write((char *)&m_yPos, sizeof(DWORD));
	m_sName.WriteToStream(pStream);
	m_sAttributes.WriteToStream(pStream);
	pStream->Write((char *)&m_iLevel, sizeof(DWORD));
	pStream->Write((char *)&m_dwID, sizeof(DWORD));

	DWORD dwCount = m_NamedGates.GetCount();
	pStream->Write((char *)&dwCount, sizeof(DWORD));
	for (i = 0; i < (int)dwCount; i++)
		{
		StarGateDesc *pDesc = (StarGateDesc *)m_NamedGates.GetValue(i);
		CString sName = m_NamedGates.GetKey(i);
		sName.WriteToStream(pStream);
		pDesc->sDestNode.WriteToStream(pStream);
		pDesc->sDestEntryPoint.WriteToStream(pStream);
		}

	dwCount = m_VariantLabels.GetCount();
	pStream->Write((char *)&dwCount, sizeof(DWORD));
	for (i = 0; i < (int)dwCount; i++)
		m_VariantLabels[i].WriteToStream(pStream);

	//	Write opaque data

	m_Data.WriteToStream(pStream);

	//	Flags

	dwSave = 0;
	dwSave |= (m_bKnown ? 0x00000001 : 0);
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	//	Write end game data

	m_sEpitaph.WriteToStream(pStream);
	m_sEndGameReason.WriteToStream(pStream);
	}