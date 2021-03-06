/***********************************************************************
  Moses - factored phrase-based language decoder
  Copyright (C) 2012- University of Edinburgh

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/

#include "ScoreFeature.h"
#include "domain.h"

using namespace std;

namespace MosesTraining
{


  const string& ScoreFeatureManager::usage() const
  {
    const static string& usage = "[--[Sparse]Domain[Indicator|Ratio|Subset|Bin] domain-file [bins]]"  ;
    return usage;
  }

  void ScoreFeatureManager::configure(const std::vector<std::string> args)
  {
    bool domainAdded = false;
    bool sparseDomainAdded = false;
    for (size_t i = 0; i < args.size(); ++i) {
      if (args[i].substr(0, 8) == "--Domain") {
        string type = args[i].substr(8);
        ++i;
        UTIL_THROW_IF(i == args.size(), ScoreFeatureArgumentException, "Missing domain file");
        string domainFile = args[i];
        UTIL_THROW_IF(domainAdded, ScoreFeatureArgumentException,
             "Only allowed one domain feature");
        if (type == "Subset") {
          m_features.push_back(ScoreFeaturePtr(new SubsetDomainFeature(domainFile)));
        } else if (type == "Ratio") {
          m_features.push_back(ScoreFeaturePtr(new RatioDomainFeature(domainFile)));
        } else if (type == "Indicator") {
          m_features.push_back(ScoreFeaturePtr(new IndicatorDomainFeature(domainFile)));
        } else {
          UTIL_THROW(ScoreFeatureArgumentException, "Unknown domain feature type " << type);
        }
        domainAdded = true;
        m_includeSentenceId = true;
      } else if (args[i].substr(0, 14) == "--SparseDomain") {
        string type = args[i].substr(14);
        ++i;
        UTIL_THROW_IF(i == args.size(), ScoreFeatureArgumentException, "Missing domain file");
        string domainFile = args[i];
        UTIL_THROW_IF(sparseDomainAdded, ScoreFeatureArgumentException,
             "Only allowed one sparse domain feature");
        if (type == "Subset") {
          m_features.push_back(ScoreFeaturePtr(new SparseSubsetDomainFeature(domainFile)));
        } else if (type == "Ratio") {
          m_features.push_back(ScoreFeaturePtr(new SparseRatioDomainFeature(domainFile)));
        } else if (type == "Indicator") {
          m_features.push_back(ScoreFeaturePtr(new SparseIndicatorDomainFeature(domainFile)));
        } else {
          UTIL_THROW(ScoreFeatureArgumentException, "Unknown domain feature type " << type);
        }
        sparseDomainAdded = true;
        m_includeSentenceId = true;
      } else {
        UTIL_THROW(ScoreFeatureArgumentException, "Unknown score argument " << args[i]);
      }
    }

  }

  bool ScoreFeatureManager::equals(const PhraseAlignment& lhs, const PhraseAlignment& rhs) const
  {
    for (size_t i = 0; i < m_features.size(); ++i) {
      if (!m_features[i]->equals(lhs, rhs)) return false;
    }
    return true;
  }

 void ScoreFeatureManager::addFeatures(const ScoreFeatureContext& context,
           std::vector<float>& denseValues,
           std::map<std::string, float>& sparseValues) const
 {
    for (size_t i = 0; i < m_features.size(); ++i) {
      m_features[i]->add(context, denseValues, sparseValues);
    }
 }
}
