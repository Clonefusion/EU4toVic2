#include "V2Country.h"
#include "Log.h"
#include "../Configuration.h"
#include "../Mappers/ReligionMapper.h"
#include "../Mappers/PartyNameMapper.h"
#include "CardinalToOrdinal.h"
#include "ParadoxParser8859_15.h"
#include "OSCompatibilityLayer.h"
#include "../EU4World/CultureGroups.h"
#include "../EU4World/World.h"
#include "../EU4World/EU4Country.h"
#include "../EU4World/EU4Relations.h"
#include "../EU4World/EU4Leader.h"
#include "../EU4World/Provinces/EU4Province.h"
#include "../Mappers/AdjacencyMapper.h"
#include "../Mappers/CountryMapping.h"
#include "../Mappers/CultureMapper.h"
#include "../Mappers/GovernmentMapper.h"
#include "../Mappers/Ideas/IdeaEffectMapper.h"
#include "../Mappers/Ideas/TechGroupsMapper.h"
#include "../Mappers/ProvinceMappings/ProvinceMapper.h"
#include "V2World.h"
#include "V2State.h"
#include "V2Province.h"
#include "V2Relations.h"
#include "V2Army.h"
#include "V2Reforms.h"
#include "V2UncivReforms.h"
#include "V2Creditor.h"
#include "V2Leader.h"
#include "V2Pop.h"
#include "V2TechSchools.h"
#include <algorithm>
#include <exception>
#include <float.h>
#include <fstream>
#include <math.h>
#include <sstream>
#include <queue>
#include <cmath>


const int MONEYFACTOR = 30;	// ducat to pound conversion rate


V2Country::V2Country(const string& countriesFileLine, const V2World* _theWorld, bool _dynamicCountry)
{
	string filename;
	int start = countriesFileLine.find_first_of('/');
	start++;
	int size = countriesFileLine.find_last_of('\"') - start;
	filename = countriesFileLine.substr(start, size);

	shared_ptr<Object> countryData = parseCountryFile(filename);

	vector<shared_ptr<Object>> partyData = countryData->getValue("party");
	for (vector<shared_ptr<Object>>::iterator itr = partyData.begin(); itr != partyData.end(); ++itr)
	{
		V2Party* newParty = new V2Party(*itr);
		parties.push_back(newParty);
	}

	theWorld			= _theWorld;
	newCountry		= false;
	dynamicCountry	= _dynamicCountry;

	tag = countriesFileLine.substr(0, 3);
	commonCountryFile	= localisation.convertCountryFileName(filename);
	std::replace(filename.begin(), filename.end(), ':', ';');
	std::replace(filename.begin(), filename.end(), '/', ' ');
	std::replace(filename.begin(), filename.end(), '\\', ' ');
	commonCountryFile = commonCountryFile;
	rulingParty			= "";

	states.clear();
	provinces.clear();

	leadership				= 0.0;
	plurality				= 0.0;
	capital					= 0;
	diploPoints				= 0.0;
	badboy					= 0.0;
	money						= 0.0;
	techSchool				= "traditional_academic";
	researchPoints			= 0.0;
	civilized				= false;
	isReleasableVassal	= true;
	inHRE						= false;
	holyRomanEmperor		= false;
	celestialEmperor		= false;
	primaryCulture			= "dummy";
	religion					= "shamanist";
	government				= "absolute_monarchy";
	nationalValue			= "nv_order";
	lastBankrupt			= date();
	bankReserves			= 0.0;
	literacy					= 0.0;

	acceptedCultures.clear();
	techs.clear();
	reactionaryIssues.clear();
	conservativeIssues.clear();
	liberalIssues.clear();
	creditors.clear();

	reforms		= nullptr;
	srcCountry	= nullptr;

	upperHouseReactionary	= 10;
	upperHouseConservative	= 65;
	upperHouseLiberal			= 25;

	uncivReforms = nullptr;

	if (parties.empty())
	{	// No parties are specified. Grab some.
		loadPartiesFromBlob();
	}

	// set a default ruling party
	for (vector<V2Party*>::iterator i = parties.begin(); i != parties.end(); i++)
	{
		if ((*i)->isActiveOn(date("1836.1.1")))
		{
			rulingParty = (*i)->name;
			break;
		}
	}

	colonyOverlord = nullptr;

	for (int i = 0; i < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++i)
	{
		unitNameCount[i] = 0;
	}

	numFactories	= 0;

}

void V2Country::loadPartiesFromBlob()
{
	std::ifstream partyFile("PartyNames.txt");
	mappers::PartyNameMapper partyNameMapper(partyFile);
	partyFile.close();

	auto partyMap = partyNameMapper.getMap();

	map<string, mappers::PartyName>::iterator partyItr;

	size_t i = 0;
	for (partyItr = partyMap.begin(); partyItr != partyMap.end(); ++partyItr)
	{
		map<string, string>::iterator languageItr;
		auto languageMap = partyItr->second.getMap();

		std::string partyKey = tag + '_' + partyItr->first;

		parties.push_back(new V2Party(partyKey, partyItr->first));
		localisation.SetPartyKey(i, partyKey);
		
		for (languageItr = languageMap.begin(); languageItr != languageMap.end(); ++languageItr)
		{
			localisation.SetPartyName(i, languageItr->first, languageItr->second);
		}
		++i;
	}

}


shared_ptr<Object> V2Country::parseCountryFile(const string& filename)
{
	string fileToParse;
	if (Utils::DoesFileExist("./blankMod/output/common/countries/" + filename))
	{
		fileToParse = "./blankMod/output/common/countries/" + filename;
	}
	else if (Utils::DoesFileExist(theConfiguration.getVic2Path() + "/common/countries/" + filename))
	{
		fileToParse = theConfiguration.getVic2Path() + "/common/countries/" + filename;
	}
	else
	{
		LOG(LogLevel::Debug) << "Could not find file common/countries/" << filename << " - skipping";
		return nullptr;
	}

	shared_ptr<Object> countryData = parser_8859_15::doParseFile(fileToParse);
	if (countryData == nullptr)
	{
		LOG(LogLevel::Warning) << "Could not parse file " << fileToParse;
	}

	return countryData;
}


V2Country::V2Country(const string& _tag, const string& _commonCountryFile, const V2World* _theWorld)
{
	theWorld = _theWorld;
	newCountry = true;
	dynamicCountry = false;

	tag					= _tag;
	commonCountryFile	= localisation.convertCountryFileName(_commonCountryFile);
	std::replace(commonCountryFile.begin(), commonCountryFile.end(), ':', ';');
	std::replace(commonCountryFile.begin(), commonCountryFile.end(), '/', ' ');
	std::replace(commonCountryFile.begin(), commonCountryFile.end(), '\\', ' ');
	commonCountryFile = commonCountryFile;
	parties.clear();
	rulingParty			= "";

	states.clear();
	provinces.clear();
	inventions.clear();

	leadership				= 0.0;
	plurality				= 0.0;
	capital					= 0;
	diploPoints				= 0.0;
	badboy					= 0.0;
	money						= 0.0;
	techSchool				= "traditional_academic";
	researchPoints			= 0.0;
	civilized				= false;
	isReleasableVassal	= true;
	inHRE						= false;
	holyRomanEmperor		= false;
	celestialEmperor		= false;
	primaryCulture			= "dummy";
	religion					= "shamanist";
	government				= "absolute_monarchy";
	nationalValue			= "nv_order";
	lastBankrupt			= date();
	bankReserves			= 0.0;
	literacy					= 0.0;

	acceptedCultures.clear();
	techs.clear();
	reactionaryIssues.clear();
	conservativeIssues.clear();
	liberalIssues.clear();
	creditors.clear();

	reforms		= nullptr;
	srcCountry	= nullptr;

	upperHouseReactionary	= 10;
	upperHouseConservative	= 65;
	upperHouseLiberal			= 25;

	uncivReforms = nullptr;

	if (parties.empty())
	{	// No parties are specified. Get some.
		loadPartiesFromBlob();
	}

	// set a default ruling party
	for (vector<V2Party*>::iterator i = parties.begin(); i != parties.end(); i++)
	{
		if ((*i)->isActiveOn(date("1836.1.1")))
		{
			rulingParty = (*i)->name;
			break;
		}
	}

	colonyOverlord = nullptr;

	for (int i = 0; i < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++i)
	{
		unitNameCount[i] = 0;
	}

	numFactories	= 0;
}


void V2Country::output() const
{
	if(!dynamicCountry)
	{
		FILE* output;
		if (fopen_s(&output, ("output/" + theConfiguration.getOutputName() + "/history/countries/" + filename).c_str(), "w") != 0)
		{
			LOG(LogLevel::Error) << "Could not create country history file " << filename;
			exit(-1);
		}

		if (capital > 0)
		{
			fprintf(output, "capital=%d\n", capital);
		}
		fprintf(output, "primary_culture = %s\n", primaryCulture.c_str());
		for (set<string>::iterator i = acceptedCultures.begin(); i != acceptedCultures.end(); i++)
		{
			fprintf(output, "culture = %s\n", i->c_str());
		}
		fprintf(output, "religion = %s\n", religion.c_str());
		fprintf(output, "government = %s\n", government.c_str());
		fprintf(output, "plurality=%f\n", plurality);
		fprintf(output, "nationalvalue=%s\n", nationalValue.c_str());
		fprintf(output, "literacy=%f\n", literacy);
		if (civilized)
		{
			fprintf(output, "civilized=yes\n");
		}
		if (!isReleasableVassal)
		{
			fprintf(output, "is_releasable_vassal=no\n");
		}
		fprintf(output, "\n");
		fprintf(output, "# Social Reforms\n");
		fprintf(output, "wage_reform = no_minimum_wage\n");
		fprintf(output, "work_hours = no_work_hour_limit\n");
		fprintf(output, "safety_regulations = no_safety\n");
		fprintf(output, "health_care = no_health_care\n");
		fprintf(output, "unemployment_subsidies = no_subsidies\n");
		fprintf(output, "pensions = no_pensions\n");
		fprintf(output, "school_reforms = no_schools\n");

		if (reforms != nullptr)
		{
			reforms->output(output);
		}
		else
		{
			fprintf(output, "# Political Reforms\n");
			fprintf(output, "slavery=yes_slavery\n");
			fprintf(output, "vote_franschise=none_voting\n");
			fprintf(output, "upper_house_composition=appointed\n");
			fprintf(output, "voting_system=jefferson_method\n");
			fprintf(output, "public_meetings=yes_meeting\n");
			fprintf(output, "press_rights=censored_press\n");
			fprintf(output, "trade_unions=no_trade_unions\n");
			fprintf(output, "political_parties=underground_parties\n");
		}
		fprintf(output, "\n");
		fprintf(output, "ruling_party=%s\n", rulingParty.c_str());
		fprintf(output, "upper_house=\n");
		fprintf(output, "{\n");
		fprintf(output, "	fascist = 0\n");
		fprintf(output, "	liberal = %d\n", upperHouseLiberal);
		fprintf(output, "	conservative = %d\n", upperHouseConservative);
		fprintf(output, "	reactionary = %d\n", upperHouseReactionary);
		fprintf(output, "	anarcho_liberal = 0\n");
		fprintf(output, "	socialist = 0\n");
		fprintf(output, "	communist = 0\n");
		fprintf(output, "}\n");
		fprintf(output, "\n");
		fprintf(output, "# Starting Consciousness\n");
		fprintf(output, "consciousness = 0\n");
		fprintf(output, "nonstate_consciousness = 0\n");
		fprintf(output, "\n");
		outputTech(output);
		if (!civilized)
		{
			if (uncivReforms != nullptr)
			{
				uncivReforms->output(output);
			}
		}
		fprintf(output, "prestige=%f\n", prestige);

		if (!decisions.empty())
		{
			fprintf(output, "\n");
			fprintf(output, "# Decisions\n");
			fprintf(output, "1820.1.1 = {\n");
			for (const auto& decision : decisions)
			{
				fprintf(output, "\tdecision = %s\n", decision.c_str());
			}
			fprintf(output, "}\n");
		}



		//fprintf(output, "	schools=\"%s\"\n", techSchool.c_str());

		fprintf(output, "oob = \"%s\"\n", (tag + "_OOB.txt").c_str());

		if (holyRomanEmperor)
		{
			fprintf(output, "set_country_flag = emperor_hre\n");
		}
		else if (inHRE)
		{
			fprintf(output, "set_country_flag = member_hre\n");
		}

		if (celestialEmperor)
		{
			fprintf(output, "set_country_flag = celestial_emperor\n");
		}

		fclose(output);

		outputOOB();
	}

	if (newCountry)
	{
		// Output common country file.
		std::ofstream commonCountryOutput("output/" + theConfiguration.getOutputName() + "/common/countries/" + commonCountryFile);
		if (!commonCountryOutput.is_open())
		{
			LOG(LogLevel::Error) << "Could not open output/" + theConfiguration.getOutputName() + "/common/countries/" + commonCountryFile;
			exit(-1);
		}
		commonCountryOutput << "graphical_culture = UsGC\n";	// default to US graphics
		commonCountryOutput << "color = { " << nationalColors.getMapColor() << " }\n";
		for (auto party : parties)
		{
			commonCountryOutput	<< '\n'
										<< "party = {\n"
										<< "    name = \"" << party->name << "\"\n"
										<< "    start_date = " << party->start_date << '\n'
										<< "    end_date = " << party->end_date << "\n\n"
										<< "    ideology = " << party->ideology << "\n\n"
										<< "    economic_policy = " << party->economic_policy << '\n'
										<< "    trade_policy = " << party->trade_policy << '\n'
										<< "    religious_policy = " << party->religious_policy << '\n'
										<< "    citizenship_policy = " << party->citizenship_policy << '\n'
										<< "    war_policy = " << party->war_policy << '\n'
										<< "}\n";
		}
	}
}


void V2Country::outputToCommonCountriesFile(FILE* output) const
{
	fprintf(output, "%s = \"countries/%s\"\n", tag.c_str(), commonCountryFile.c_str());
}


void V2Country::outputLocalisation(std::ostream& output) const
{
	 output << localisation;
}


void V2Country::outputTech(FILE* output) const
{
	fprintf(output, "\n");
	fprintf(output, "# Technologies\n");
	for (vector<string>::const_iterator itr = techs.begin(); itr != techs.end(); ++itr)
	{
		fprintf(output, "%s", itr->c_str()); fprintf(output, " = 1\n");
	}
}


void V2Country::outputElection(FILE* output) const
{
	date electionDate = date("1832.1.1");
	fprintf(output, "	last_election=%s\n", electionDate.toString().c_str());
}


void V2Country::outputOOB() const
{
	std::ofstream output("output/" + theConfiguration.getOutputName() + "/history/units/" + tag + "_OOB.txt");
	if (!output.is_open())
	{
		std::runtime_error exception("Could not create OOB file " + tag + "_OOB.txt");
		throw exception;
	}

	output << "#Sphere of Influence\n";
	output << "\n";
	for (auto relation: relations)
	{
		output << relation.second;
	}

	output << "\n";
	output << "#Leaders\n";
	for (auto leader: leaders)
	{
		output << leader;
	}

	output << "\n";
	output << "#Armies\n";
	for (auto army: armies)
	{
		output << army;
	}

	output.close();
}


void V2Country::initFromEU4Country(
	const EU4::Regions& eu4Regions,
	std::shared_ptr<EU4::Country> _srcCountry,
	const std::unique_ptr<Vic2::TechSchools>& techSchools,
	const map<int, int>& leaderMap,
	const mappers::CultureMapper& cultureMapper,
	const mappers::CultureMapper& slaveCultureMapper,
	const mappers::IdeaEffectMapper& ideaEffectMapper,
	const mappers::ReligionMapper& religionMapper,
	const mappers::ProvinceMapper& provinceMapper,
	const mappers::GovernmentMapper& governmentMapper
) {
	srcCountry = _srcCountry;

	if (false == srcCountry->getRandomName().empty())
	{
		newCountry = true;
	}

	auto possibleFilename = Utils::GetFileFromTag("./blankMod/output/history/countries/", tag);
	if (!possibleFilename)
	{
		possibleFilename = Utils::GetFileFromTag(theConfiguration.getVic2Path() + "/history/countries/", tag);
	}

	if (!possibleFilename)
	{
		string countryName	= commonCountryFile;
		int lastSlash			= countryName.find_last_of("/");
		countryName				= countryName.substr(lastSlash + 1, countryName.size());
		filename					= tag + " - " + countryName;
	}
	else
	{
		filename = *possibleFilename;
	}

	// Color
	nationalColors = srcCountry->getNationalColors();

	// Localisation
	localisation.SetTag(tag);
	localisation.ReadFromCountry(*srcCountry);

	// Capital
	int oldCapital = srcCountry->getCapital();
	auto potentialCapitals = provinceMapper.getVic2ProvinceNumbers(oldCapital);
	if (potentialCapitals.size() > 0)
	{
		capital = *potentialCapitals.begin();
	}

	// in HRE
	inHRE					= srcCountry->getInHRE();
	holyRomanEmperor	= srcCountry->getHolyRomanEmperor();

	// celestial emperor
	celestialEmperor = srcCountry->getCelestialEmperor();

	// religion
	setReligion(_srcCountry, religionMapper);

	// cultures
	setPrimaryAndAcceptedCultures(_srcCountry, cultureMapper, eu4Regions);

	// Government
	determineGovernmentType(_srcCountry, ideaEffectMapper, governmentMapper);

	// Apply government effects to reforms
	finalizeInvestments(_srcCountry, ideaEffectMapper);

	//  Politics
	resolvePolitics();

	// Generate Reforms
	reforms		=  new V2Reforms(this, srcCountry);

	// Relations
	generateRelations(_srcCountry);

	// Literacy and Tech school
	calculateLiteracy(_srcCountry);
	determineTechSchool(techSchools);

	// Misc
	buildCanals(_srcCountry);
}

void V2Country::setReligion(std::shared_ptr<EU4::Country> srcCountry, const mappers::ReligionMapper& religionMapper)
{
	string srcReligion = srcCountry->getReligion();
	if (srcReligion.size() > 0)
	{
		std::optional<std::string> match = religionMapper.getVic2Religion(srcReligion);
		if (!match)
		{
			LOG(LogLevel::Warning) << "No religion mapping defined for " << srcReligion
				<< " (" << srcCountry->getTag() << " -> " << tag << ')';
		}
		else
		{
			religion = *match;
		}
	}
}

void V2Country::setPrimaryAndAcceptedCultures(std::shared_ptr<EU4::Country> srcCountry, const mappers::CultureMapper& cultureMapper, const EU4::Regions& eu4Regions)
{
	int oldCapital = srcCountry->getCapital();

	// primary culture
	string srcCulture = srcCountry->getPrimaryCulture();

	if (srcCulture.size() > 0)
	{
		std::optional<std::string> matched = cultureMapper.cultureMatch(
			eu4Regions,
			srcCulture,
			religion,
			oldCapital,
			srcCountry->getTag()
		);
		if (!matched)
		{
			LOG(LogLevel::Warning) << "No culture mapping defined for " << srcCulture
				<< " (" << srcCountry->getTag() << " -> " << tag << ')';
		}
		else
		{
			primaryCulture = *matched;
		}
	}

	//accepted cultures
	vector<string> srcAceptedCultures = srcCountry->getAcceptedCultures();
	auto culturalUnion = srcCountry->getCulturalUnion();
	if (culturalUnion)
	{
		for (auto unionCulture : culturalUnion->getCultures())
		{
			srcAceptedCultures.push_back(unionCulture.first);
		}
	}
	for (auto srcCulture : srcAceptedCultures)
	{
		std::optional<std::string> dstCulture;
		dstCulture = cultureMapper.cultureMatch(
			eu4Regions,
			srcCulture,
			religion,
			oldCapital,
			srcCountry->getTag()
		);
		if (dstCulture)
		{
			if (primaryCulture != *dstCulture)
			{
				acceptedCultures.insert(*dstCulture);
			}
		}
		else
		{
			LOG(LogLevel::Warning) << "No culture mapping defined for " << srcCulture
				<< " (" << srcCountry->getTag() << " -> " << tag << ')';
		}
	}

}

void V2Country::determineGovernmentType(std::shared_ptr<EU4::Country> srcCountry, const mappers::IdeaEffectMapper& ideaEffectMapper, const mappers::GovernmentMapper& governmentMapper)
{
	government = governmentMapper.matchGovernment(srcCountry->getGovernment());

	for (auto reformStr : srcCountry->getReforms())
	{
		std::string enforce = ideaEffectMapper.getEnforceFromIdea(reformStr, 7);
		if (!enforce.empty())
		{
			LOG(LogLevel::Debug) << "Forcing government " << enforce << " on " << tag;
			government = enforce;
		}
	}
	
	// almost but not quite

	if (srcCountry->isRevolutionary())
	{
		government = "bourgeois_dictatorship";
	}

}

void V2Country::finalizeInvestments(std::shared_ptr<EU4::Country> srcCountry, const mappers::IdeaEffectMapper& ideaEffectMapper)
{
	// Collect and finalize all idea/reform/government effects. We have combined reforms + ideas incoming, but lack government component (the last 33%)
	// Resulting scores for all of these will be between 0 and 10, with 5 being average and supposed to be ignored.
	// Each point above or below 5 should alter absolute values by 10%.

	armyInvestment = (2 * srcCountry->getArmyInvestment() + ideaEffectMapper.getArmyFromIdea(government, 8)) / 3;
	navyInvestment = (2 * srcCountry->getNavyInvestment() + ideaEffectMapper.getNavyFromIdea(government, 8)) / 3;
	commerceInvestment = (2 * srcCountry->getCommerceInvestment() + ideaEffectMapper.getCommerceFromIdea(government, 8)) / 3;
	industryInvestment = (2 * srcCountry->getIndustryInvestment() + ideaEffectMapper.getIndustryFromIdea(government, 8)) / 3;
	cultureInvestment = (2 * srcCountry->getCultureInvestment() + ideaEffectMapper.getCultureFromIdea(government, 8)) / 3;
	slaveryInvestment = (2 * srcCountry->getSlaveryInvestment() + ideaEffectMapper.getSlaveryFromIdea(government, 8)) / 3;
	upper_house_compositionInvestment = (2 * srcCountry->getUpper_house_compositionInvestment() + ideaEffectMapper.getUpper_house_compositionFromIdea(government, 8)) / 3;
	vote_franchiseInvestment = (2 * srcCountry->getVote_franchiseInvestment() + ideaEffectMapper.getVote_franchiseFromIdea(government, 8)) / 3;
	voting_systemInvestment = (2 * srcCountry->getVoting_systemInvestment() + ideaEffectMapper.getVoting_systemFromIdea(government, 8)) / 3;
	public_meetingsInvestment = (2 * srcCountry->getPublic_meetingsInvestment() + ideaEffectMapper.getPublic_meetingsFromIdea(government, 8)) / 3;
	press_rightsInvestment = (2 * srcCountry->getPress_rightsInvestment() + ideaEffectMapper.getPress_rightsFromIdea(government, 8)) / 3;
	trade_unionsInvestment = (2 * srcCountry->getTrade_unionsInvestment() + ideaEffectMapper.getTrade_unionsFromIdea(government, 8)) / 3;
	political_partiesInvestment = (2 * srcCountry->getPolitical_partiesInvestment() + ideaEffectMapper.getPolitical_partiesFromIdea(government, 8)) / 3;
	libertyInvestment = (2 * srcCountry->getLibertyInvestment() + ideaEffectMapper.getLibertyFromIdea(government, 8)) / 3;
	equalityInvestment = (2 * srcCountry->getEqualityInvestment() + ideaEffectMapper.getEqualityFromIdea(government, 8)) / 3;
	orderInvestment = (2 * srcCountry->getOrderInvestment() + ideaEffectMapper.getOrderFromIdea(government, 8)) / 3;
	literacyInvestment = (2 * srcCountry->getLiteracyInvestment() + ideaEffectMapper.getLiteracyFromIdea(government, 8)) / 3;
	reactionaryInvestment = (2 * srcCountry->getReactionaryInvestment() + ideaEffectMapper.getReactionaryFromIdea(government, 8)) / 3;
	liberalInvestment = (2 * srcCountry->getLiberalInvestment() + ideaEffectMapper.getLiberalFromIdea(government, 8)) / 3;

}

void V2Country::resolvePolitics()
{
	upperHouseReactionary = static_cast<int>(5 * (1 + (reactionaryInvestment - 5) * 20 / 100));
	upperHouseLiberal = static_cast<int>(10 * (1 + (liberalInvestment - 5) * 20 / 100));
	upperHouseConservative = 100 - (upperHouseReactionary + upperHouseLiberal);

	if (srcCountry->isRevolutionary())
	{
		upperHouseReactionary = static_cast<int>(upperHouseReactionary / 3);
		upperHouseLiberal = static_cast<int>(upperHouseLiberal * 3);
		upperHouseConservative = 100 - (upperHouseReactionary + upperHouseLiberal);
		LOG(LogLevel::Debug) << tag << " is revolutionary! ";
	}

	string idealogy;

	double liberalEffect = liberalInvestment - 5;
	double reactionaryEffect = reactionaryInvestment - 5;

	if (srcCountry->isRevolutionary())
	{
		liberalEffect += 10;
	}

	if (liberalEffect >= 2 * reactionaryEffect)
	{
		idealogy = "liberal";
		upperHouseLiberal = static_cast<int>(upperHouseLiberal * 1.1);
		upperHouseConservative = 100 - (upperHouseReactionary + upperHouseLiberal);
	}
	else if (reactionaryEffect >= 2 * liberalEffect)
	{
		idealogy = "reactionary";
		upperHouseReactionary = static_cast<int>(upperHouseReactionary * 1.1);
		upperHouseConservative = 100 - (upperHouseReactionary + upperHouseLiberal);
	}
	else
	{
		idealogy = "conservative";
	}

	for (vector<V2Party*>::iterator i = parties.begin(); i != parties.end(); i++)
	{
		if ((*i)->isActiveOn(date("1836.1.1")) && ((*i)->ideology == idealogy))
		{
			rulingParty = (*i)->name;
			break;
		}
	}
}

void V2Country::generateRelations(std::shared_ptr<EU4::Country> srcCountry)
{
	auto srcRelations = srcCountry->getRelations();
	for (auto srcRelation : srcRelations)
	{
		const std::string& V2Tag = mappers::CountryMappings::getVic2Tag(srcRelation.second->getCountry());
		if (!V2Tag.empty())
		{
			V2Relations newRelations(V2Tag, srcRelation.second);
			relations.insert(std::make_pair(V2Tag, newRelations));
		}
	}
}

void V2Country::calculateLiteracy(std::shared_ptr<EU4::Country> srcCountry)
{
	literacy = 0.4;

	if (
		((srcCountry->getReligion().compare("protestant") == 0) ||
		(srcCountry->getReligion().compare("anglican") == 0) ||
			(srcCountry->getReligion().compare("confucian") == 0) ||
			(srcCountry->getReligion().compare("reformed") == 0))
		)
	{
		literacy += 0.1;
	}

	if (srcCountry->hasModifier("the_school_establishment_act"))
	{
		literacy += 0.05;
	}
	if (srcCountry->hasModifier("sunday_schools"))
	{
		literacy += 0.05;
	}
	if (srcCountry->hasModifier("the_education_act"))
	{
		literacy += 0.05;
	}
	if (srcCountry->hasModifier("monastic_education_system"))
	{
		literacy += 0.05;
	}
	if (srcCountry->hasModifier("western_embassy_mission"))
	{
		literacy += 0.05;
	}

	// Universities grant at most 10% literacy, with either having 10 or when having them in 10% of provinces, whichever comes sooner.
	// Colleges do half of what universities do.

	int numProvinces = 0;
	int numColleges = 0;
	int numUniversities = 0;
	vector<EU4::Province*> provinces = srcCountry->getProvinces();
	numProvinces = provinces.size();
	for (vector<EU4::Province*>::iterator i = provinces.begin(); i != provinces.end(); ++i)
	{
		if ((*i)->hasBuilding("college"))
		{
			numColleges++;
		}
		if ((*i)->hasBuilding("university"))
		{
			numUniversities++;
		}
	}

	double collegeBonus1 = 0;
	double universityBonus1 = 0;
	if (numProvinces > 0)
	{
		collegeBonus1 = numColleges / numProvinces;
		universityBonus1 = numUniversities * 2 / numProvinces;
	}
	double collegeBonus2 = numColleges * 0.005;
	double universityBonus2 = numUniversities * 0.01;

	double collegeBonus = min(max(collegeBonus1, collegeBonus2), 0.05);
	double universityBonus = min(max(universityBonus1, universityBonus2), 0.1);

	literacy += collegeBonus + universityBonus;

	if (literacy > theConfiguration.getMaxLiteracy())
	{
		literacy = theConfiguration.getMaxLiteracy();
	}

	// Finally apply collective national literacy modifier.

	literacy *= (1 + (literacyInvestment - 5) * 10 / 100);

}

void V2Country::determineTechSchool(const std::unique_ptr<Vic2::TechSchools>& techSchools)
{
	techSchool = techSchools->findBestTechSchool(
		armyInvestment - 5,
		commerceInvestment - 5,
		cultureInvestment - 5,
		industryInvestment - 5,
		navyInvestment - 5
	);
}

void V2Country::buildCanals(std::shared_ptr<EU4::Country> srcCountry)
{
	for (const auto& prov : srcCountry->getProvinces())
	{
		if (prov->hasGreatProject("suez_canal"))
		{
			LOG(LogLevel::Debug) << "Building Suez Canal in: " << prov->getName();
			decisions.push_back("build_suez_canal");
		}
		if (prov->hasGreatProject("kiel_canal"))
		{
			LOG(LogLevel::Debug) << "Building Kiel Canal in: " << prov->getName();
			decisions.push_back("build_kiel_canal");
		}
		if (prov->hasGreatProject("panama_canal"))
		{
			LOG(LogLevel::Debug) << "Building Panama Canal in: " << prov->getName();
			decisions.push_back("build_panama_canal");
		}
	}

}


// used only for countries which are NOT converted (i.e. unions, dead countries, etc)
void V2Country::initFromHistory()
{
	// Ping unreleasable_tags for this country's TAG
	ifstream unreleasableTags;
	string inputBuffer;

	unreleasableTags.open("./unreleasable_tags.txt");
	while (getline(unreleasableTags, inputBuffer))
	{
		if (inputBuffer.rfind(tag, 0) == 0)
		{
			LOG(LogLevel::Debug) << "Found " << tag << " in unreleasables.";
			isReleasableVassal = false;
		}
	}
	unreleasableTags.close();

	string fullFilename;

	auto possibleFilename = Utils::GetFileFromTag("./blankMod/output/history/countries/", tag);
	if (possibleFilename)
	{
		filename = *possibleFilename;
		fullFilename = "./blankMod/output/history/countries/" + filename;
	}
	else
	{
		possibleFilename = Utils::GetFileFromTag(theConfiguration.getVic2Path() + "/history/countries/", tag);
		if (possibleFilename)
		{
			filename = *possibleFilename;
			fullFilename = theConfiguration.getVic2Path() + "/history/countries/" + filename;
		}
	}
	if (!possibleFilename)
	{
		string countryName	= commonCountryFile;
		int lastSlash			= countryName.find_last_of("/");
		countryName				= countryName.substr(lastSlash + 1, countryName.size());
		filename					= tag + " - " + countryName;
		return;
	}

	shared_ptr<Object> obj = parser_8859_15::doParseFile(fullFilename.c_str());
	if (obj == nullptr)
	{
		LOG(LogLevel::Error) << "Could not parse file " << fullFilename;
		exit(-1);
	}

	vector<shared_ptr<Object>> results = obj->getValue("primary_culture");
	if (results.size() > 0)
	{
		primaryCulture = results[0]->getLeaf();
	}

	results = obj->getValue("culture");
	for (vector<shared_ptr<Object>>::iterator itr = results.begin(); itr != results.end(); ++itr)
	{
		acceptedCultures.insert((*itr)->getLeaf());
	}

	results = obj->getValue("religion");
	if (results.size() > 0)
	{
		religion = results[0]->getLeaf();
	}

	results = obj->getValue("government");
	if (results.size() > 0)
	{
		government = results[0]->getLeaf();
	}

	results = obj->getValue("civilized");
	if (results.size() > 0)
	{
		civilized = (results[0]->getLeaf() == "yes");
	}
	// don't bother if already false by override.
	if (isReleasableVassal)
	{
		results = obj->getValue("is_releasable_vassal");
		if (results.size() > 0)
		{
			isReleasableVassal = (results[0]->getLeaf() == "yes");
		}
	}

	results = obj->getValue("nationalvalue");
	if (results.size() > 0)
	{
		nationalValue = results[0]->getLeaf();
	}
	else
	{
		nationalValue = "nv_order";
	}

	results = obj->getValue("capital");
	if (results.size() > 0)
	{
		capital = atoi(results[0]->getLeaf().c_str());
	}
}


void V2Country::addProvince(V2Province* _province)
{
	auto itr = provinces.find(_province->getNum());
	if (itr != provinces.end())
	{
		LOG(LogLevel::Error) << "Inserting province " << _province->getNum() << " multiple times (addProvince())";
	}
	provinces.insert(make_pair(_province->getNum(), _province));
}


static set<int> getPortBlacklist()
{
	// hack for naval bases.  not ALL naval bases are in port provinces, and if you spawn a navy at a naval base in
	// a non-port province, Vicky crashes....
	static set<int> port_blacklist;
	if (port_blacklist.size() == 0)
	{
		int temp = 0;
		ifstream s("port_blacklist.txt");
		while (s.good() && !s.eof())
		{
			s >> temp;
			port_blacklist.insert(temp);
		}
		s.close();
	}
	return port_blacklist;
}


std::vector<int> V2Country::getPortProvinces(
	const std::vector<int>& locationCandidates,
	std::map<int, V2Province*> allProvinces
) {
	std::set<int> port_blacklist = getPortBlacklist();

	std::vector<int> unblockedCandidates;
	for (auto candidate: locationCandidates)
	{
		if (port_blacklist.count(candidate) == 0)
		{
			unblockedCandidates.push_back(candidate);
		}
	}

	std::vector<int> coastalProvinces;
	for (auto& candidate: unblockedCandidates)
	{
		std::map<int, V2Province*>::iterator province = allProvinces.find(candidate);
		if (province != allProvinces.end())
		{
			if (province->second->isCoastal())
			{
				coastalProvinces.push_back(candidate);
			}
		}
	}

	return coastalProvinces;
}


void V2Country::addState(V2State* newState)
{
	int				highestNavalLevel	= 0;
	unsigned int	hasHighestLevel	= -1;
	bool				hasNavalBase		= false;

	states.push_back(newState);
	vector<V2Province*> newProvinces = newState->getProvinces();

	std::vector<int> newProvinceNums;
	for (const auto& province: newProvinces)
	{
		newProvinceNums.push_back(province->getNum());
	}
	auto portProvinces = getPortProvinces(newProvinceNums, provinces);

	for (unsigned int i = 0; i < newProvinces.size(); i++)
	{
		auto itr = provinces.find(newProvinces[i]->getNum());
		if (itr == provinces.end())
		{
			provinces.insert(make_pair(newProvinces[i]->getNum(), newProvinces[i]));
		}

		// find the province with the highest naval base level
		if ((theConfiguration.getVic2Gametype() == "HOD") || (theConfiguration.getVic2Gametype() == "HoD-NNM"))
		{
			int navalLevel = 0;
			const EU4::Province* srcProvince = newProvinces[i]->getSrcProvince();
			if (srcProvince != nullptr)
			{
				if (srcProvince->hasBuilding("shipyard"))
				{
					navalLevel += 1;
				}
				if (srcProvince->hasBuilding("grand_shipyard"))
				{
					navalLevel += 1;
				}
				if (srcProvince->hasBuilding("naval_arsenal"))
				{
					navalLevel += 1;
				}
				if (srcProvince->hasBuilding("naval_base"))
				{
					navalLevel += 1;
				}
			}
			bool isPortProvince = std::find(portProvinces.begin(), portProvinces.end(), newProvinces[i]->getNum()) != portProvinces.end();
			if (navalLevel > highestNavalLevel && isPortProvince)
			{
				highestNavalLevel	= navalLevel;
				hasHighestLevel	= i;
			}
			newProvinces[i]->setNavalBaseLevel(0);
		}
	}
	if (((theConfiguration.getVic2Gametype() == "HOD") || (theConfiguration.getVic2Gametype() == "HoD-NNM")) && (highestNavalLevel > 0))
	{
		newProvinces[hasHighestLevel]->setNavalBaseLevel(1);
	}
}


//#define TEST_V2_PROVINCES
void V2Country::convertArmies(
	const std::map<int,int>& leaderIDMap,
	double cost_per_regiment[static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories)],
	const std::map<int, V2Province*>& allProvinces,
	std::vector<int> port_whitelist,
	const mappers::ProvinceMapper& provinceMapper
) {
#ifndef TEST_V2_PROVINCES
	if (srcCountry == nullptr)
	{
		return;
	}
	if (provinces.size() == 0)
	{
		return;
	}

	// set up armies with whatever regiments they deserve, rounded down
	// and keep track of the remainders for later
	double countryRemainder[static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories)] = { 0.0 };
	std::vector<EU4::EU4Army> sourceArmies = srcCountry->getArmies();
	for (std::vector<EU4::EU4Army>::iterator aitr = sourceArmies.begin(); aitr != sourceArmies.end(); ++aitr)
	{
		V2Army army(*aitr, leaderIDMap);

		for (int rc = static_cast<int>(EU4::REGIMENTCATEGORY::infantry); rc < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++rc)
		{
			int typeStrength = aitr->getTotalTypeStrength(static_cast<EU4::REGIMENTCATEGORY>(rc));
			if (typeStrength == 0) // no regiments of this type
				continue;

			// if we have ships, we must be a navy
			bool isNavy = (rc >= static_cast<int>(EU4::REGIMENTCATEGORY::heavy_ship));
			army.setNavy(isNavy);

			double	regimentCount		= typeStrength / cost_per_regiment[rc];
			int		regimentsToCreate	= (int)floor(regimentCount);
			double	regimentRemainder	= regimentCount - regimentsToCreate;
			countryRemainder[rc] += regimentRemainder;
			army.setArmyRemainders(static_cast<EU4::REGIMENTCATEGORY>(rc), army.getArmyRemainder(static_cast<EU4::REGIMENTCATEGORY>(rc)) + regimentRemainder);

			for (int i = 0; i < regimentsToCreate; ++i)
			{
				if (
					addRegimentToArmy(army, static_cast<EU4::REGIMENTCATEGORY>(rc), allProvinces, provinceMapper) !=
					addRegimentToArmyResult::success
					)
				{
					// couldn't add, dissolve into pool
					countryRemainder[rc] += 1.0;
					army.setArmyRemainders(static_cast<EU4::REGIMENTCATEGORY>(rc), army.getArmyRemainder(static_cast<EU4::REGIMENTCATEGORY>(rc)) + 1.0);
				}
			}
		}

		auto locationCandidates = provinceMapper.getVic2ProvinceNumbers(aitr->getLocation());
		if (locationCandidates.size() == 0)
		{
			LOG(LogLevel::Debug) << "Army or Navy " << aitr->getName() << " assigned to unmapped province " << aitr->getLocation() << "; dissolving to pool";
			int regimentCounts[static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories)] = { 0 };
			army.getRegimentCounts(regimentCounts);
			for (int rc = static_cast<int>(EU4::REGIMENTCATEGORY::infantry); rc < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++rc)
			{
				countryRemainder[rc] += regimentCounts[rc];
			}
			continue;
		}
		else if ((locationCandidates.size() == 1) && (*locationCandidates.begin() == 0))
		{
			LOG(LogLevel::Debug) << "Army or Navy " << aitr->getName() << " assigned to dropped province " << aitr->getLocation() << "; dissolving to pool";
			int regimentCounts[static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories)] = { 0 };
			army.getRegimentCounts(regimentCounts);
			for (int rc = static_cast<int>(EU4::REGIMENTCATEGORY::infantry); rc < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++rc)
			{
				countryRemainder[rc] += regimentCounts[rc];
			}
			continue;
		}
		bool usePort = false;
		// guarantee that navies are assigned to sea provinces, or land provinces with naval bases
		if (army.getNavy())
		{
			auto pitr = allProvinces.find(*locationCandidates.begin());
			if (pitr != allProvinces.end())
			{
				usePort = true;
			}
			if (usePort)
			{
				locationCandidates = getPortProvinces(locationCandidates, allProvinces);
				if (locationCandidates.size() == 0)
				{
					LOG(LogLevel::Debug) << "Navy " << aitr->getName() << " assigned to EU4 province " << aitr->getLocation() << " which has no corresponding V2 port provinces; dissolving to pool";
					int regimentCounts[static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories)] = { 0 };
					army.getRegimentCounts(regimentCounts);
					for (int rc = static_cast<int>(EU4::REGIMENTCATEGORY::infantry); rc < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++rc)
					{
						countryRemainder[rc] += regimentCounts[rc];
					}
					continue;
				}
			}
		}

		int selectedLocation = locationCandidates[int(locationCandidates.size() * ((double)rand() / RAND_MAX))];
		if (army.getNavy() && usePort)
		{
			vector<int>::iterator white = std::find(port_whitelist.begin(), port_whitelist.end(), selectedLocation);
			if (white == port_whitelist.end())
			{
				LOG(LogLevel::Warning) << "Assigning navy to non-whitelisted port province " << selectedLocation << " - if the save crashes, try blacklisting this province";
			}
		}
		army.setLocation(selectedLocation);
		armies.push_back(army);
	}

	// allocate the remainders from the whole country to the armies according to their need, rounding up
	for (int rc = static_cast<int>(EU4::REGIMENTCATEGORY::infantry); rc < static_cast<int>(EU4::REGIMENTCATEGORY::num_reg_categories); ++rc)
	{
                int attempts = 0;
		while (countryRemainder[rc] > 0.0 && attempts < 100)
		{
			V2Army* army = getArmyForRemainder(static_cast<EU4::REGIMENTCATEGORY>(rc));
                        attempts++;
			if (army == nullptr)
			{
				LOG(LogLevel::Debug) << "No suitable army or navy found for " << tag << "'s pooled regiments of " << EU4::RegimentCategoryTypes[static_cast<EU4::REGIMENTCATEGORY>(rc)];
				break;
			}
			switch (addRegimentToArmy(*army, static_cast<EU4::REGIMENTCATEGORY>(rc), allProvinces, provinceMapper))
			{
				case addRegimentToArmyResult::success:
					countryRemainder[rc] -= 1.0;
					army->setArmyRemainders(static_cast<EU4::REGIMENTCATEGORY>(rc), army->getArmyRemainder(static_cast<EU4::REGIMENTCATEGORY>(rc)) - 1.0);
					break;
				case addRegimentToArmyResult::retry:
					break;
				case addRegimentToArmyResult::doNotRetry:
					LOG(LogLevel::Debug) << "Disqualifying army/navy " << army->getName() << " from receiving more " << EU4::RegimentCategoryTypes[static_cast<EU4::REGIMENTCATEGORY>(rc)] << " from the pool";
					army->setArmyRemainders(static_cast<EU4::REGIMENTCATEGORY>(rc), -2000.0);
					break;
			}
		}
	}

#else // ifdef TEST_V2_PROVINCES
	// output one big ship to each V2 province that's neither whitelisted nor blacklisted, but only 10 at a time per nation
	// output from this mode is used to build whitelist and blacklist files
	set<int> port_blacklist = getPortBlacklist();
	int n_tests = 0;
	for (auto itr = provinces.begin(); (itr != provinces.end()) && (n_tests < 50); ++itr)
	{
		V2Province* pitr = itr->second;
		if (pitr->isCoastal())
		{
			set<int>::iterator black = std::find(port_blacklist.begin(), port_blacklist.end(), pitr->getNum());
			if (black != port_blacklist.end())
				continue;

			V2Army army = V2Army::makeTestNavy(itr->first);
			armies.push_back(army);

			vector<int>::iterator white = std::find(port_whitelist.begin(), port_whitelist.end(), pitr->getNum());
			if (white == port_whitelist.end())
			{
				++n_tests;
				ofstream s("port_greylist.txt", ios_base::app);
				s << pitr->getNum() << "\n";
				s.close();
			}
		}
	}
	LOG(LogLevel::Debug) << "Output " << n_tests << " test ships.";
#endif
}


std::tuple<double, double, double> V2Country::getNationalValueScores() 
{
	double orderScore = 0.0;
	double libertyScore = 0.0;
	double equalityScore = 0.0;

	if (srcCountry)
	{
		orderScore += srcCountry->getOrderInvestment() - 5.0;
		libertyScore += srcCountry->getLibertyInvestment() - 5.0;
		equalityScore += srcCountry->getEqualityInvestment() - 5.0;
	}

	return make_tuple(libertyScore, equalityScore, orderScore);
}


void V2Country::addRelation(V2Relations& newRelation)
{
	relations.insert(std::make_pair(newRelation.getTag(), newRelation));
}


void V2Country::absorbVassal(V2Country* vassal)
{
	Log(LogLevel::Debug) << "\t" << tag << " is absorbing " << vassal->getTag();

	// change province ownership and add owner cores if needed
	map<int, V2Province*> vassalProvinces = vassal->getProvinces();
	for (auto provItr = vassalProvinces.begin(); provItr != vassalProvinces.end(); provItr++)
	{
		provItr->second->setOwner(tag);
		provItr->second->setController(tag);
		provItr->second->addCore(tag);
	}
	vassal->provinces.clear();

	// accept cultures from the vassal
	if (primaryCulture != vassal->getPrimaryCulture())
	{
		acceptedCultures.insert(vassal->getPrimaryCulture());
	}
	set<string> cultures = vassal->getAcceptedCultures();
	for (auto itr: cultures)
	{
		if (primaryCulture != itr)
		{
			acceptedCultures.insert(itr);
		}
	}

	// take vassal's armies
	srcCountry->takeArmies(vassal->getSourceCountry());

	// assume the vassal's decisions (just canals, at the moment)
	for (const auto& decision : vassal->decisions)
	{
		decisions.push_back(decision);
	}
	vassal->decisions.clear();
}


void V2Country::setColonyOverlord(V2Country* colony)
{
	colonyOverlord = colony;
}


V2Country* V2Country::getColonyOverlord()
{
	return colonyOverlord;
}


std::string	V2Country::getColonialRegion()
{
	return srcCountry->getColonialRegion();
}


static bool FactoryCandidateSortPredicate(const pair<double, V2State*>& lhs, const pair<double, V2State*>& rhs)
{
	if (lhs.first != rhs.first)
		return lhs.first > rhs.first;
	return lhs.second->getID() < rhs.second->getID();
}


bool V2Country::addFactory(V2Factory* factory)
{
	// check factory techs
	string requiredTech = factory->getRequiredTech();
	if (requiredTech != "")
	{
		vector<string>::iterator itr = find(techs.begin(), techs.end(), requiredTech);
		if (itr == techs.end())
		{
			LOG(LogLevel::Debug) << tag << " rejected " << factory->getTypeName() << " (missing required tech: " << requiredTech << ')';
			return false;
		}
	}

	// check factory inventions
	if ((theConfiguration.getVic2Gametype() == "vanilla") || (theConfiguration.getVic2Gametype() == "AHD"))
	{
		if (inventions.count(factory->getRequiredInvention()) != 0)
		{
			LOG(LogLevel::Debug) << tag << " rejected " << factory->getTypeName() << " (missing required invention: " << factory->getRequiredInvention() << ')';
			return false;
		}
	}

	// find a state to add the factory to, which meets the factory's requirements
	vector<pair<double, V2State*>> candidates;
	for (vector<V2State*>::iterator itr = states.begin(); itr != states.end(); ++itr)
	{
		if ( (*itr)->isColonial() )
		{
			continue;
		}

		if ( (*itr)->getFactoryCount() >= 8 )
		{
			continue;
		}

		if (factory->requiresCoastal())
		{
			if ( !(*itr)->isCoastal() )
				continue;
		}

		if ( !(*itr)->hasLandConnection() )
		{
			continue;
		}

		double candidateScore	 = (*itr)->getSuppliedInputs(factory) * 100;
		candidateScore				-= static_cast<double>((*itr)->getFactoryCount()) * 10;
		candidateScore				+= (*itr)->getManuRatio();
		candidates.push_back(pair<double, V2State*>(candidateScore, (*itr) ));
	}

	sort(candidates.begin(), candidates.end(), FactoryCandidateSortPredicate);

	if (candidates.size() == 0)
	{
		LOG(LogLevel::Debug) << tag << " rejected " << factory->getTypeName() << " (no candidate states)";
		return false;
	}

	V2State* target = candidates[0].second;
	target->addFactory(factory);
	LOG(LogLevel::Debug) << tag << " accepted " << factory->getTypeName() << " (" << candidates.size() << " candidate states)";
	numFactories++;
	return true;
}


void V2Country::addRailroadtoCapitalState()
{
	for (vector<V2State*>::iterator i = states.begin(); i != states.end(); i++)
	{
		if ( (*i)->provInState(capital) )
		{
			(*i)->addRailroads();
		}
	}
}


void V2Country::convertUncivReforms(int techGroupAlgorithm, double topTech, int topInstitutions, const mappers::TechGroupsMapper& techGroupsMapper)
{
	enum civConversion { older, newer };
	switch (techGroupAlgorithm)
	{
	case(older):
		oldCivConversionMethod();
	break;

	case(newer):
		newCivConversionMethod(topTech, topInstitutions, techGroupsMapper);
	break;
	}
}

void V2Country::oldCivConversionMethod() // civilisation level conversion method for games up to 1.18
{
	if ((srcCountry != nullptr) && ((theConfiguration.getVic2Gametype() == "AHD") || (theConfiguration.getVic2Gametype() == "HOD") || (theConfiguration.getVic2Gametype() == "HoD-NNM")))
	{
		if ((srcCountry->getTechGroup() == "western") || (srcCountry->getTechGroup() == "high_american") || (srcCountry->getTechGroup() == "eastern") || (srcCountry->getTechGroup() == "ottoman") || (srcCountry->numEmbracedInstitutions() >= 7))//civilised, do nothing
		{
			civilized = true;
		}
		else
		{
			civilized = false;
		}
		if ((srcCountry->getTechGroup() == "western") || (srcCountry->getTechGroup() == "high_american") || (srcCountry->getTechGroup() == "eastern") || (srcCountry->getTechGroup() == "ottoman") || (srcCountry->numEmbracedInstitutions() >= 7))
		{
			// civilized, do nothing
		}
		else if (((srcCountry->getTechGroup() == "north_american") || (srcCountry->getTechGroup() == "mesoamerican") ||	(srcCountry->getTechGroup() == "south_american") || (srcCountry->getTechGroup() == "new_world") || (srcCountry->getTechGroup() == "andean")) && (srcCountry->numEmbracedInstitutions() <= 3))
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << " and " << srcCountry->numEmbracedInstitutions() << " institutions. westernization at 0%";
			uncivReforms = new V2UncivReforms(0, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else if ((srcCountry->getIsolationism() == 0) && (srcCountry->numEmbracedInstitutions() >= 6))
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << ", " << srcCountry->numEmbracedInstitutions() << " institutions and an isolationism of " << srcCountry->numEmbracedInstitutions() << ". westernization at 50%";
			uncivReforms = new V2UncivReforms(50, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else if ((srcCountry->getTechGroup() == "muslim") || (srcCountry->numEmbracedInstitutions() >= 6))
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << " and " << srcCountry->numEmbracedInstitutions() << " institutions. westernization at 44%";
			uncivReforms = new V2UncivReforms(44, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else if ((srcCountry->getTechGroup() == "indian") || (srcCountry->getIsolationism() == 0))
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << ", " << srcCountry->numEmbracedInstitutions() << " institutions and an isolationism of " << srcCountry->numEmbracedInstitutions() << ".  Westernization at 40%";
			uncivReforms = new V2UncivReforms(40, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else if ((srcCountry->getTechGroup() == "chinese") || (srcCountry->numEmbracedInstitutions() == 5))
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << " and " << srcCountry->numEmbracedInstitutions() << " institutions. westernization at 36%";
			uncivReforms = new V2UncivReforms(36, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else if (srcCountry->getTechGroup() == "nomad_group")
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << " and " << srcCountry->numEmbracedInstitutions() << " institutions. westernization at 30%";
			uncivReforms = new V2UncivReforms(30, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else if ((srcCountry->getTechGroup() == "sub_saharan") || (srcCountry->getTechGroup() == "central_african") || (srcCountry->getTechGroup() == "east_african") || (srcCountry->numEmbracedInstitutions() == 4))
		{
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			LOG(LogLevel::Debug) << "Setting unciv reforms for " << tag << " who has tech group " << srcCountry->getTechGroup() << " and " << srcCountry->numEmbracedInstitutions() << " institutions. westernization at 20%";
			uncivReforms = new V2UncivReforms(20, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
		else
		{
			LOG(LogLevel::Warning) << "Unhandled tech group (" << srcCountry->getTechGroup() << " with " << srcCountry->numEmbracedInstitutions() << " institutions) for " << tag << " - giving no reforms";
			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech();
			double militaryDev = srcCountry->getMilTech() / totalTechs;
			double socioEconDev = srcCountry->getAdmTech() / totalTechs;
			uncivReforms = new V2UncivReforms(0, militaryDev, socioEconDev, this);
			government = "absolute_monarchy";
		}
	}
}

void V2Country::newCivConversionMethod(double topTech, int topInsitutions, const mappers::TechGroupsMapper& techGroupsMapper) // civilisation level conversion method for games after 1.18
{
	{
		if (srcCountry != nullptr) {

			double totalTechs = srcCountry->getMilTech() + srcCountry->getAdmTech() + srcCountry->getDipTech();

			// set civilisation cut off for 6 techs behind the the tech leader (30 years behind tech)
			// set number for civilisation level based on techs and institutions
			// at 31 techs behind completely unciv
			// each institution behind is equivalent to 2 techs behind

			double civLevel = (totalTechs + 31 - topTech) * 4;
			civLevel = civLevel + (static_cast<double>(srcCountry->numEmbracedInstitutions()) - topInsitutions) * 8;
			if (civLevel > 100) civLevel = 100;
			if (civLevel < 0) civLevel = 0;

			string techGroup = srcCountry->getTechGroup();

			if (theConfiguration.getEuroCentrism() == Configuration::EUROCENTRISM::EuroCentric)
			{
				literacy *= (1 + (static_cast<double>(techGroupsMapper.getLiteracyFromTechGroup(techGroup)) - 5.0) * 10.0 / 100.0);
				civLevel = civLevel * (static_cast<double>(techGroupsMapper.getWesternizationFromTechGroup(techGroup)) / 10.0);
			}

			literacy = literacy * theConfiguration.getMaxLiteracy() * (pow(10, (civLevel / 100) * 0.9 + 0.1) / 10);

			if (civLevel == 100)
			{
				civilized = true;
			}
			else
			{
				civilized = false;
			}

			if (((theConfiguration.getVic2Gametype() == "AHD") || (theConfiguration.getVic2Gametype() == "HOD") || (theConfiguration.getVic2Gametype() == "HoD-NNM")) && (civilized == false))
			{
				totalTechs = totalTechs - srcCountry->getDipTech();
				double militaryDev = srcCountry->getMilTech() / totalTechs;
				double socioEconDev = srcCountry->getAdmTech() / totalTechs;
				uncivReforms = new V2UncivReforms((int)(civLevel + 0.5), militaryDev, socioEconDev, this);
				government = "absolute_monarchy";
			}
		}
	}
}

void V2Country::convertLandlessReforms(V2Country* capOwner)
{
	if (capOwner->isCivilized())
	{
		civilized = true;
	}
	else
	{
		civilized = false;
		V2UncivReforms* uncivReforms = capOwner->getUncivReforms();
	}
}


void V2Country::setupPops(
	double popWeightRatio,
	int popConversionAlgorithm,
	const std::map<std::string, std::shared_ptr<EU4::Country>>& theEU4Countries,
	const mappers::ProvinceMapper& provinceMapper
) {
	if (states.size() < 1) // skip entirely for empty nations
		return;

	// create the pops
	for (auto itr = provinces.begin(); itr != provinces.end(); ++itr)
	{
		itr->second->doCreatePops(popWeightRatio, this, popConversionAlgorithm, theEU4Countries, provinceMapper);
	}

	// output statistics on pops
	/*map<string, long int> popsData;
	for (auto provItr = provinces.begin(); provItr != provinces.end(); provItr++)
	{
		auto pops = provItr->second->getPops();
		for (auto popsItr = pops.begin(); popsItr != pops.end(); popsItr++)
		{
			auto popItr = popsData.find( (*popsItr)->getType() );
			if (popItr == popsData.end())
			{
				long int newPopSize = 0;
				pair<map<string, long int>::iterator, bool> newIterator = popsData.insert(make_pair((*popsItr)->getType(), 0));
				popItr = newIterator.first;
			}
			popItr->second += (*popsItr)->getSize();
		}
	}
	long int totalPops = 0;
	for (auto dataItr = popsData.begin(); dataItr != popsData.end(); dataItr++)
	{
		totalPops += dataItr->second;
	}

	for (auto dataItr = popsData.begin(); dataItr != popsData.end(); dataItr++)
	{
		double popsPercent = static_cast<double>(dataItr->second) / totalPops;
		string filename = dataItr->first;
		filename += ".csv";
		FILE* dataFile = fopen(filename.c_str(), "a");
		if (dataFile != nullptr)
		{
			fprintf(dataFile, "%s,%d,%f\n", tag.c_str(), dataItr->second, popsPercent);
			fclose(dataFile);
		}
	}*/
}


void V2Country::setArmyTech(double normalizedScore)
{
	if ((theConfiguration.getVic2Gametype() != "vanilla") && !civilized)
		return;
}


void V2Country::setNavyTech(double normalizedScore)
{
	if ((theConfiguration.getVic2Gametype() != "vanilla") && !civilized)
		return;
}


void V2Country::setCommerceTech(double normalizedScore)
{
	if ((theConfiguration.getVic2Gametype() != "vanilla") && !civilized)
		return;
}


void V2Country::setIndustryTech(double normalizedScore)
{
	if ((theConfiguration.getVic2Gametype() != "vanilla") && !civilized)
		techs.push_back("mechanized_mining");
		return;
}


void V2Country::setCultureTech(double normalizedScore)
{
	if ((theConfiguration.getVic2Gametype() != "vanilla") && !civilized)
		return;
}

string V2Country::getLocalName()
{
	return localisation.GetLocalName();
}


std::optional<V2Relations> V2Country::getRelations(std::string withWhom) const
{
	auto relation = relations.find(withWhom);
	if (relation != relations.end())
	{
		return relation->second;
	}
	else
	{
		return {};
	}
}


void V2Country::addLoan(string creditor, double size, double interest)
{
	map<string, V2Creditor*>::iterator itr = creditors.find(creditor);
	if (itr != creditors.end())
	{
			itr->second->addLoan(size, interest);
	}
	else
	{
		V2Creditor* cred = new V2Creditor(creditor);
		cred->addLoan(size, interest);
		creditors.insert(make_pair(creditor, cred));
	}
}


// return values: 0 = success, -1 = retry from pool, -2 = do not retry
addRegimentToArmyResult V2Country::addRegimentToArmy(
	V2Army& army,
	EU4::REGIMENTCATEGORY rc,
	std::map<int, V2Province*> allProvinces,
	const mappers::ProvinceMapper& provinceMapper
) {
	V2Regiment reg(rc);
	std::optional<int> eu4Home = army.getSourceArmy().getProbabilisticHomeProvince(rc);
	if (!eu4Home)
	{
		LOG(LogLevel::Debug) << "Army/navy " << army.getName() << " has no valid home provinces for " << EU4::RegimentCategoryTypes[rc] << "; dissolving to pool";
		return addRegimentToArmyResult::doNotRetry;
	}
	auto homeCandidates = provinceMapper.getVic2ProvinceNumbers(*eu4Home);
	if (homeCandidates.size() == 0)
	{
		LOG(LogLevel::Debug) << EU4::RegimentCategoryTypes[rc] << " unit in army/navy " << army.getName() << " has unmapped home province " << *eu4Home << " - dissolving to pool";
		army.getSourceArmy().blockHomeProvince(*eu4Home);
		return addRegimentToArmyResult::retry;
	}
	if (*homeCandidates.begin() == 0)
	{
		LOG(LogLevel::Debug) << EU4::RegimentCategoryTypes[rc] << " unit in army/navy " << army.getName() << " has dropped home province " << *eu4Home << " - dissolving to pool";
		army.getSourceArmy().blockHomeProvince(*eu4Home);
		return addRegimentToArmyResult::retry;
	}
	V2Province* homeProvince = nullptr;
	if (army.getNavy())
 	{
		// Navies should only get homes in port provinces
		homeCandidates = getPortProvinces(homeCandidates, allProvinces);
		if (homeCandidates.size() != 0)
		{
			std::vector<int>::const_iterator it(homeCandidates.begin());
			std::advance(it, int(homeCandidates.size() * ((double)rand() / RAND_MAX)));
			int homeProvinceID = *it;
			map<int, V2Province*>::iterator pitr = allProvinces.find(homeProvinceID);
			if (pitr != allProvinces.end())
			{
				homeProvince = pitr->second;
			}
		}
	}
	else
	{
		// Armies should get a home in the candidate most capable of supporting them
		std::vector<V2Province*> sortedHomeCandidates;
		for (auto candidate: homeCandidates)
		{
			std::map<int, V2Province*>::iterator pitr = allProvinces.find(candidate);
			if (pitr != allProvinces.end())
			{
				sortedHomeCandidates.push_back(pitr->second);
			}
		}
		sort(sortedHomeCandidates.begin(), sortedHomeCandidates.end(), ProvinceRegimentCapacityPredicate);
		if (sortedHomeCandidates.size() == 0)
		{
			LOG(LogLevel::Debug) << "No valid home for a " << tag << " " << EU4::RegimentCategoryTypes[rc] << " regiment - dissolving regiment to pool";
			// all provinces in a given province map have the same owner, so the source home was bad
			army.getSourceArmy().blockHomeProvince(*eu4Home);
			return addRegimentToArmyResult::retry;
		}
		homeProvince = sortedHomeCandidates[0];
		if (homeProvince->getOwner() != tag) // TODO: find a way of associating these units with a province owned by the proper country
		{
			map<int, V2Province*>	openProvinces = allProvinces;
			queue<int>					goodProvinces;

			map<int, V2Province*>::iterator openItr = openProvinces.find(homeProvince->getNum());
			homeProvince = nullptr;
			if ( (openItr != openProvinces.end()) && (provinces.size() > 0) )
			{
				goodProvinces.push(openItr->first);
				openProvinces.erase(openItr);

				do
				{
					int currentProvince = goodProvinces.front();
					goodProvinces.pop();
					auto adjacencies = mappers::adjacencyMapper::getVic2Adjacencies(currentProvince);
					if (adjacencies)
					{
						for (auto adjacency: *adjacencies)
						{
							auto openItr = openProvinces.find(adjacency);
							if (openItr == openProvinces.end())
							{
								continue;
							}
							if (openItr->second->getOwner() == tag)
							{
								homeProvince = openItr->second;
							}
							goodProvinces.push(openItr->first);
							openProvinces.erase(openItr);
						}
					}
				} while ((goodProvinces.size() > 0) && (homeProvince == nullptr));
			}
			if (homeProvince == nullptr)
			{
				LOG(LogLevel::Debug) << "V2 province " << sortedHomeCandidates[0]->getNum() << " is home for a " << tag << " " << EU4::RegimentCategoryTypes[rc] << " regiment, but belongs to " << sortedHomeCandidates[0]->getOwner() << " - dissolving regiment to pool";
				// all provinces in a given province map have the same owner, so the source home was bad
				army.getSourceArmy().blockHomeProvince(*eu4Home);
				return addRegimentToArmyResult::retry;
			}
			return addRegimentToArmyResult::success;
		}

		// Armies need to be associated with pops
		V2Pop* soldierPop = homeProvince->getSoldierPopForArmy();
		if (nullptr == soldierPop)
		{
			// if the old home province was colonized and can't support the unit, try turning it into an "expeditionary" army
			if (homeProvince->wasColony())
			{
				V2Province* expSender = getProvinceForExpeditionaryArmy();
				if (expSender)
				{
					V2Pop* expSoldierPop = expSender->getSoldierPopForArmy();
					if (nullptr != expSoldierPop)
					{
						homeProvince = expSender;
						soldierPop = expSoldierPop;
					}
				}
			}
		}
		if (nullptr == soldierPop)
		{
			soldierPop = homeProvince->getSoldierPopForArmy(true);
		}
		reg.setHome(homeProvince->getNum());
	}
	if (homeProvince != nullptr)
	{
		reg.setName(homeProvince->getRegimentName(rc));
	}
	else
	{
		reg.setName(getRegimentName(rc));
	}
	army.addRegiment(reg);
	return addRegimentToArmyResult::success;
}


// find the army most in need of a regiment of this category
V2Army* V2Country::getArmyForRemainder(EU4::REGIMENTCATEGORY rc)
{
	V2Army* retval = nullptr;
	double retvalRemainder = -1000.0;
	for (auto& army: armies)
	{
		// only add units to armies that originally had units of the same category
		if (army.getSourceArmy().getTotalTypeStrength(rc) > 0)
		{
			if (army.getArmyRemainder(rc) > retvalRemainder)
			{
				retvalRemainder = army.getArmyRemainder(rc);
				retval = &army;
			}
		}
	}

	return retval;
}


bool ProvinceRegimentCapacityPredicate(V2Province* prov1, V2Province* prov2)
{
	return (prov1->getAvailableSoldierCapacity() > prov2->getAvailableSoldierCapacity());
}


V2Province* V2Country::getProvinceForExpeditionaryArmy()
{
	vector<V2Province*> candidates;
	for (auto pitr = provinces.begin(); pitr != provinces.end(); ++pitr)
	{
		if ( (pitr->second->getOwner() == tag) && !pitr->second->wasColony() && !pitr->second->wasInfidelConquest()
			&& ( pitr->second->hasCulture(primaryCulture, 0.5) ) && ( pitr->second->getPops("soldiers").size() > 0) )
		{
			candidates.push_back(pitr->second);
		}
	}
	if (candidates.size() > 0)
	{
		sort(candidates.begin(), candidates.end(), ProvinceRegimentCapacityPredicate);
		return candidates[0];
	}
	return nullptr;
}


string V2Country::getRegimentName(EU4::REGIMENTCATEGORY rc)
{
	// galleys turn into light ships; count and name them identically
	if (rc == EU4::REGIMENTCATEGORY::galley)
		rc = EU4::REGIMENTCATEGORY::light_ship;

	stringstream str;
	str << ++unitNameCount[static_cast<int>(rc)] << CardinalToOrdinal(unitNameCount[static_cast<int>(rc)]); // 1st, 2nd, etc
	string adjective = localisation.GetLocalAdjective();
	if (adjective == "")
	{
		str << " ";
	}
	else
	{
		str << " " << adjective << " ";
	}
	switch (rc)
	{
	case EU4::REGIMENTCATEGORY::artillery:
		str << "Artillery";
		break;
	case EU4::REGIMENTCATEGORY::infantry:
		str << "Infantry";
		break;
	case EU4::REGIMENTCATEGORY::cavalry:
		str << "Cavalry";
		break;
	case EU4::REGIMENTCATEGORY::heavy_ship:
		str << "Man'o'war";
		break;
	case EU4::REGIMENTCATEGORY::light_ship:
		str << "Frigate";
		break;
	case EU4::REGIMENTCATEGORY::transport:
		str << "Clipper Transport";
		break;
	}
	return str.str();
}
