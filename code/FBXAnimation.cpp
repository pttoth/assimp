/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2012, assimp team
All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the 
following conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/

/** @file  FBXAnimation.cpp
 *  @brief Assimp::FBX::AnimationCurve, Assimp::FBX::AnimationCurveNode, 
 *         Assimp::FBX::AnimationLayer, Assimp::FBX::AnimationStack 
 */
#include "AssimpPCH.h"

#ifndef ASSIMP_BUILD_NO_FBX_IMPORTER

#include "FBXParser.h"
#include "FBXDocument.h"
#include "FBXImporter.h"
#include "FBXImportSettings.h"
#include "FBXDocumentUtil.h"
#include "FBXProperties.h"

namespace Assimp {
namespace FBX {

	using namespace Util;

// ------------------------------------------------------------------------------------------------
AnimationCurve::AnimationCurve(uint64_t id, const Element& element, const std::string& name, const Document& doc)
: Object(id, element, name)
{
	const Scope& sc = GetRequiredScope(element);
	const Element& KeyTime = GetRequiredElement(sc,"KeyTime");
	const Element& KeyValueFloat = GetRequiredElement(sc,"KeyValueFloat");

	ReadVectorDataArray(keys, KeyTime);
	ReadVectorDataArray(values, KeyValueFloat);

	if(keys.size() != values.size()) {
		DOMError("the number of key times does not match the number of keyframe values",&KeyTime);
	}
	
	// check if the key times are well-ordered
	if(!std::equal(keys.begin(), keys.end() - 1, keys.begin() + 1, std::less<KeyTimeList::value_type>())) {
		DOMError("the keyframes are not in ascending order",&KeyTime);
	}

	const Element* KeyAttrDataFloat = sc["KeyAttrDataFloat"];
	if(KeyAttrDataFloat) {
		ReadVectorDataArray(attributes, *KeyAttrDataFloat);
	}

	const Element* KeyAttrFlags = sc["KeyAttrFlags"];
	if(KeyAttrFlags) {
		ReadVectorDataArray(flags, *KeyAttrFlags);
	}
}


// ------------------------------------------------------------------------------------------------
AnimationCurve::~AnimationCurve()
{

}


// ------------------------------------------------------------------------------------------------
AnimationCurveNode::AnimationCurveNode(uint64_t id, const Element& element, const std::string& name, const Document& doc)
: Object(id, element, name)
, target()
{
	const Scope& sc = GetRequiredScope(element);
	props = GetPropertyTable(doc,"AnimationCurveNode.FbxAnimCurveNode",element,sc);

	{
	// resolve attached animation curves
	const std::vector<const Connection*>& conns = doc.GetConnectionsByDestinationSequenced(ID(),"AnimationCurve");

	BOOST_FOREACH(const Connection* con, conns) {

		// link should go for a property
		if (!con->PropertyName().length()) {
			continue;
		}

		const Object* const ob = con->SourceObject();
		if(!ob) {
			DOMWarning("failed to read source object for AnimationCurve->AnimationCurveNode link, ignoring",&element);
			continue;
		}

		const AnimationCurve* const anim = dynamic_cast<const AnimationCurve*>(ob);
		if(!anim) {
			DOMWarning("source object for ->AnimationCurveNode link is not an AnimationCurve",&element);
			continue;
		}
		
		curves[con->PropertyName()] = anim;
	}

	}{

	// find target node
	const std::vector<const Connection*>& conns = doc.GetConnectionsBySourceSequenced(ID(),"Model");

	BOOST_FOREACH(const Connection* con, conns) {

		// link should go for a property
		if (!con->PropertyName().length()) {
			continue;
		}

		const Object* ob = con->DestinationObject();
		if(!ob) {
			DOMWarning("failed to read destination object for AnimationCurveNode->Model link, ignoring",&element);
			continue;
		}

		target = dynamic_cast<const Model*>(ob);
		if(!target) {
			continue;
		}

		prop = con->PropertyName();
		break;
	}
	}
	if(!target) {
		DOMError("failed to resolve target model for animation node",&element);
	}
}


// ------------------------------------------------------------------------------------------------
AnimationCurveNode::~AnimationCurveNode()
{

}


// ------------------------------------------------------------------------------------------------
AnimationLayer::AnimationLayer(uint64_t id, const Element& element, const std::string& name, const Document& doc)
: Object(id, element, name)
{
	const Scope& sc = GetRequiredScope(element);
	props = GetPropertyTable(doc,"AnimationLayer.FbxAnimLayer",element,sc);

	// resolve attached animation nodes
	const std::vector<const Connection*>& conns = doc.GetConnectionsByDestinationSequenced(ID(),"AnimationCurveNode");
	nodes.reserve(conns.size());

	BOOST_FOREACH(const Connection* con, conns) {

		// link should not go to a property
		if (con->PropertyName().length()) {
			continue;
		}

		const Object* const ob = con->SourceObject();
		if(!ob) {
			DOMWarning("failed to read source object for AnimationCurveNode->AnimationLayer link, ignoring",&element);
			continue;
		}

		const AnimationCurveNode* const anim = dynamic_cast<const AnimationCurveNode*>(ob);
		if(!anim) {
			DOMWarning("source object for ->AnimationLayer link is not an AnimationCurveNode",&element);
			continue;
		}
		nodes.push_back(anim);
	}
}


// ------------------------------------------------------------------------------------------------
AnimationLayer::~AnimationLayer()
{

}

// ------------------------------------------------------------------------------------------------
AnimationStack::AnimationStack(uint64_t id, const Element& element, const std::string& name, const Document& doc)
: Object(id, element, name)
{
	const Scope& sc = GetRequiredScope(element);
	props = GetPropertyTable(doc,"AnimationStack.FbxAnimStack",element,sc);

	// resolve attached animation layers
	const std::vector<const Connection*>& conns = doc.GetConnectionsByDestinationSequenced(ID(),"AnimationLayer");
	layers.reserve(conns.size());

	BOOST_FOREACH(const Connection* con, conns) {

		// link should not go to a property
		if (con->PropertyName().length()) {
			continue;
		}

		const Object* const ob = con->SourceObject();
		if(!ob) {
			DOMWarning("failed to read source object for AnimationLayer->AnimationStack link, ignoring",&element);
			continue;
		}

		const AnimationLayer* const anim = dynamic_cast<const AnimationLayer*>(ob);
		if(!anim) {
			DOMWarning("source object for ->AnimationStack link is not an AnimationLayer",&element);
			continue;
		}
		layers.push_back(anim);
	}
}


// ------------------------------------------------------------------------------------------------
AnimationStack::~AnimationStack()
{

}

} //!FBX
} //!Assimp

#endif