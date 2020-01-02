/*Copyright(c) 2019 The Paradox Game Converters Project

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */



#include "EU4Province.h"
#include "ProvinceModifier.h"
#include "../EU4Country.h"
#include "../Religions/Religions.h"
#include "Log.h"
#include "ParserHelpers.h"
#include "../../Configuration.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <cmath>



const double BUILDING_COST_TO_WEIGHT_RATIO = 0.02;



EU4::Province::Province(
	const std::string& numString,
	std::istream& theStream,
	const Buildings& buildingTypes,
	const Modifiers& modifierTypes
) {
	registerKeyword(std::regex("name"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString nameString(theStream);
		name = nameString.getString();
	});
	registerKeyword(std::regex("base_tax"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleDouble baseTaxDouble(theStream);
		baseTax = baseTaxDouble.getDouble();
	});
	registerKeyword(std::regex("base_production"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleDouble baseProductionDouble(theStream);
		baseProduction = baseProductionDouble.getDouble();
	});
	registerKeyword(std::regex("base_manpower"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleDouble manpowerDouble(theStream);
		manpower = manpowerDouble.getDouble();
	});
	registerKeyword(std::regex("manpower"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleDouble manpowerDouble(theStream);
		manpower = manpowerDouble.getDouble();
	});
	registerKeyword(std::regex("owner"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString ownerStringString(theStream);
		ownerString = ownerStringString.getString();
	});
	registerKeyword(std::regex("controller"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString controllerStringString(theStream);
		controllerString = controllerStringString.getString();
	});
	registerKeyword(std::regex("cores"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::stringList coresStrings(theStream);
		for (auto coreString : coresStrings.getStrings())
		{
			cores.insert(coreString);
		}
	});
	registerKeyword(std::regex("core"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString coresString(theStream);
		cores.insert(coresString.getString());
	});
    registerKeyword(std::regex("territorial_core"),[this](const std::string& unused, std::istream& theStream) {
        commonItems::ignoreItem(unused, theStream);
        territorialCore = true;
    });
	registerKeyword(std::regex("hre"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString hreString(theStream);
		if (hreString.getString() == "yes")
		{
			inHRE = true;
		}
	});
	registerKeyword(std::regex("is_city"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString cityString(theStream);
		if (cityString.getString() == "yes")
		{
			city = true;
		}
	});
	registerKeyword(std::regex("colonysize"), [this](const std::string & unused, std::istream & theStream) {
		commonItems::ignoreItem(unused, theStream);
		colony = true;
	});
	registerKeyword(std::regex("original_coloniser"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::ignoreItem(unused, theStream);
		hadOriginalColoniser = true;
	});
	registerKeyword(std::regex("history"), [this](const std::string& unused, std::istream& theStream) {
		provinceHistory = std::make_unique<ProvinceHistory>(theStream);
	});
	registerKeyword(std::regex("buildings"), [this](const std::string& unused, std::istream& theStream) {
		buildings = std::make_unique<ProvinceBuildings>(theStream);
	});
	registerKeyword(std::regex("great_projects"), [this](const std::string& unused, std::istream& theStream) {
		greatProjects = std::make_unique<GreatProjects>(theStream);
	});
	registerKeyword(std::regex("modifier"), [this](const std::string& unused, std::istream& theStream) {
		ProvinceModifier modifier(theStream);
		modifiers.insert(modifier.getModifier());
	});
	registerKeyword(std::regex("trade_goods"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleString tradeGoodsString(theStream);
		tradeGoods = tradeGoodsString.getString();
	});
	registerKeyword(std::regex("center_of_trade"), [this](const std::string& unused, std::istream& theStream) {
		commonItems::singleInt cotLevelInt(theStream);
		centerOfTradeLevel = cotLevelInt.getInt();
	});
	registerKeyword(std::regex("[a-zA-Z0-9_]+"), commonItems::ignoreItem);

	parseStream(theStream);

	num = 0 - stoi(numString);

	// for old versions of EU4 (< 1.12), copy tax to production if necessary
	if ((baseProduction == 0.0f) && (baseTax > 0.0f))
	{
		baseProduction = baseTax;
	}
	if (!provinceHistory)
	{
		std::stringstream input;
		provinceHistory = std::make_unique<ProvinceHistory>(input);
	}

	determineProvinceWeight(buildingTypes, modifierTypes);
}


bool EU4::Province::wasInfidelConquest(const std::string& ownerReligion, const EU4::Religions& allReligions) const
{
	return provinceHistory->wasInfidelConquest(allReligions, ownerReligion, num);
}


bool EU4::Province::hasBuilding(const std::string& building) const
{
	if (buildings && buildings->hasBuilding(building))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool EU4::Province::hasGreatProject(const std::string& greatProject) const
{
	if (greatProjects && greatProjects->hasGreatProject(greatProject))
	{
		return true;
	}
	else
	{
		return false;
	}
}


double EU4::Province::getCulturePercent(const std::string& culture) const
{
	double culturePercent = 0.0f;

	for (auto pop: provinceHistory->getPopRatios())
	{
		if (pop.getCulture() == culture)
		{
			culturePercent += pop.getLowerRatio();
		}
	}

	return culturePercent;
}

void EU4::Province::makeState(double p)
{
	stated = true;
	prosperity = p;
}

void EU4::Province::determineProvinceWeight(const Buildings& buildingTypes, const Modifiers& modifierTypes)
{
	double manpower_weight = manpower;

	BuildingWeightEffects buildingWeightEffects = getProvBuildingWeight(buildingTypes, modifierTypes);
	buildingWeight = buildingWeightEffects.buildingWeight;
	double manpowerModifier = buildingWeightEffects.manpowerModifier;
	double manufactoriesValue = buildingWeightEffects.manufactoriesValue;
	double productionEfficiency = buildingWeightEffects.productionEfficiency;
	double taxModifier = buildingWeightEffects.taxModifier;
	double tradeGoodsSizeModifier = buildingWeightEffects.tradeGoodsSizeModifier;
	double tradePower = buildingWeightEffects.tradePower;
	double tradeValue = buildingWeightEffects.tradeValue;
	double tradeEfficiency = buildingWeightEffects.tradeEfficiency;
	double tradeSteering = buildingWeightEffects.tradeSteering;
	double taxEfficiency = buildingWeightEffects.taxEfficiency;
	double devbonus = 0.0;

	// Check tag, ex. TIB has goods_produced +0.05
	// This needs to be hard coded unless there's some other way of figuring out modded national ambitions/ideas
	//if (ownerString == "TIB")
	//{
	//	tradeGoodsSizeModifier += 0.05;
	//}

	double xyz = 0.0;
	
	if ((taxModifier) >= 0.6)
	{
		xyz += 0.6;
	}
	if (((taxModifier) >= 0.4) && ((taxModifier) <= 0.5))
	{
		xyz += 0.4;
	}

	double goodsProduced = ((baseProduction * 0.2) + manufactoriesValue ) * (1 + tradeGoodsSizeModifier + 0.03);
	goodsProduced = std::max(0.0, goodsProduced);

	// idea effects
	/*if ( (owner !=  NULL) && (owner->hasNationalIdea("bureaucracy")) )
	{
		taxEfficiency += 0.10;
	}
	if ( (owner !=  NULL) && (owner->hasNationalIdea("smithian_economics")) )
	{
		productionEfficiency += 0.10;
	}*/

	// manpower
	manpower_weight *= 25;
	//manpower_weight += manpowerModifier;
	manpower_weight *= ((1 + manpowerModifier) / 25); // should work now as intended

	//LOG(LogLevel::Info) << "Manpower Weight: " << manpower_weight;

	double total_tx = (baseTax * (1 + taxModifier + 0.65) + taxEfficiency * (1 + taxModifier + 0.15));
	double production_eff_tech = 0.2; // used to be 1.0

	double total_trade_value = ((getTradeGoodPrice() * goodsProduced) + tradeValue) * (1 + tradeEfficiency);
	double production_income = total_trade_value * (1 + production_eff_tech + productionEfficiency + 0.8);
	//LOG(LogLevel::Info) << "province name: " << this->getProvName() 
	//	<< " trade good: " << tradeGoods 
	//	<< " Price: " << getTradeGoodPrice() 
	//	<< " trade value: " << trade_value 
	//	<< " trade value eff: " 
	//	<< (1 + trade_value_eff) 
	//	<< " goods produced: " << goods_produced 
	//	<< " production eff: " << production_eff 
	//	<< " Production: " << production_income;
	
	if ((baseTax + baseProduction + manpower) >= 10)
	{
		devbonus += 1;
	}

	if ((baseTax + baseProduction + manpower) >= 20)
	{
		devbonus += 2;
	}

	if ((baseTax + baseProduction + manpower) >= 30)
	{
		devbonus += 3;
	}

	if ((baseTax + baseProduction + manpower) >= 40)
	{
		devbonus += 4;
	}

	if ((baseTax + baseProduction + manpower) >= 50)
	{
		devbonus += 6;
	}

	if ((baseTax + baseProduction + manpower) >= 60)
	{
		devbonus += 8;
	}

	if ((baseTax + baseProduction + manpower) >= 70)
	{
		devbonus += 10;
	}

	if ((baseTax + baseProduction + manpower) >= 80)
	{
		devbonus += 12;
	}

	if ((baseTax + baseProduction + manpower) >= 90)
	{
		devbonus += 15;
	}

	if ((baseTax + baseProduction + manpower) >= 15)
	{
		devbonus += 1;
	}

	if ((baseTax + baseProduction + manpower) >= 25)
	{
		devbonus += 2;
	}

	if ((baseTax + baseProduction + manpower) >= 35)
	{
		devbonus += 3;
	}

	if ((baseTax + baseProduction + manpower) >= 45)
	{
		devbonus += 4;
	}

	if ((baseTax + baseProduction + manpower) >= 55)
	{
		devbonus += 6;
	}

	if ((baseTax + baseProduction + manpower) >= 65)
	{
		devbonus += 8;
	}

	if ((baseTax + baseProduction + manpower) >= 75)
	{
		devbonus += 10;
	}

	if ((baseTax + baseProduction + manpower) >= 85)
	{
		devbonus += 12;
	}

	total_tx *= 1;
	manpower_weight *= 1;
	production_income *= 1;

	taxIncome = total_tx;
	productionIncome = production_income;
	manpowerWeight = manpower_weight;
	
	// dev modifier
	//devModifier = ( baseTax + baseProduction + manpower );
	//devDelta = devModifier - provinceHistory->getOriginalDevelopment();
	modifierWeight = buildingWeight + manpower_weight + production_income + total_tx;

	tradeSteering *= ( baseTax + baseProduction + manpower );
	devModifier = ( baseTax + baseProduction + manpower );
	
	totalWeight = modifierWeight + ( 2 * devModifier + 3 * devbonus  + tradeSteering );

	if (modifierWeight > 0)
	{
		// provinces with modifierweights under 10 (underdeveloped with no buildings) get a penalty for popShaping.
		modifierWeight = (std::log10(modifierWeight) - 1) * 10;
	}

	if (ownerString == "")
	{
		totalWeight = 0;
		modifierWeight = 0;
	}

	provinceStats.setGoodsProduced(goodsProduced);
	provinceStats.setPrice(getTradeGoodPrice());
	provinceStats.setTradeEfficiency(1 + tradeEfficiency);
	provinceStats.setProductionEfficiency(1 + productionEfficiency);
	provinceStats.setTradeValue(tradeValue);
	provinceStats.setTradeValue(production_income);
	provinceStats.setBaseTax(baseTax);
	provinceStats.setBuildingsIncome(xyz);
	provinceStats.setBuildingsIncome(taxModifier);
	provinceStats.setTaxEfficiency(taxEfficiency);
	provinceStats.setTotalTaxIncome(total_tx);
	provinceStats.setTotalTradeValue(total_trade_value);
}


double EU4::Province::getTradeGoodPrice() const
{
	// Trade goods
	/*
	chinaware
	grain
	fish
	tabacco
	iron
	copper
	cloth
	ivory
	slaves
	salt
	wool
	fur
	gold
	sugar
	naval_supplies
	tea
	coffee
	spices
	wine
	cocoa
	silk
	dyes
	tropical_wood
	*/
	//LOG(LogLevel::Info) << "Trade Goods Price";
	double tradeGoodsPrice = 0;

	if (tradeGoods == "chinaware")
	{
		tradeGoodsPrice = 3.04;
	}
	else if (tradeGoods == "grain")
	{
		tradeGoodsPrice = 2.03;
	}
	else if (tradeGoods == "fish")
	{
		tradeGoodsPrice = 2.11;
	}
	else if (tradeGoods == "tobacco")
	{
		tradeGoodsPrice = 3.84;
	}
	else if (tradeGoods == "iron")
	{
		tradeGoodsPrice = 3.74;
	}
	else if (tradeGoods == "copper")
	{
		tradeGoodsPrice = 3.79;
	}
	else if (tradeGoods == "cloth")
	{
		tradeGoodsPrice = 3.62;
	}
	else if (tradeGoods == "slaves")
	{
		tradeGoodsPrice = 2.68;
	}
	else if (tradeGoods == "salt")
	{
		tradeGoodsPrice = 3.24;
	}
	else if (tradeGoods == "gold")
	{
		tradeGoodsPrice = 6;
	}
	else if (tradeGoods == "fur")
	{
		tradeGoodsPrice = 3.14;
	}
	else if (tradeGoods == "sugar")
	{
		tradeGoodsPrice = 3.71;
	}
	else if (tradeGoods == "naval_supplies")
	{
		tradeGoodsPrice = 2.35;
	}
	else if (tradeGoods == "tea")
	{
		tradeGoodsPrice = 2.68;
	}
	else if (tradeGoods == "coffee")
	{
		tradeGoodsPrice = 2.97;
	}
	else if (tradeGoods == "spices")
	{
		tradeGoodsPrice = 3.68;
	}
	else if (tradeGoods == "wine")
	{
		tradeGoodsPrice = 2.75;
	}
	else if (tradeGoods == "cocoa")
	{
		tradeGoodsPrice = 4.39;
	}
	else if (tradeGoods == "ivory")
	{
		tradeGoodsPrice = 4.19;
	}
	else if (tradeGoods == "wool")
	{
		tradeGoodsPrice = 2.7;
	}
	else if (tradeGoods == "cotton")
	{
		tradeGoodsPrice = 3.56;
	}
	else if (tradeGoods == "dyes")
	{
		tradeGoodsPrice = 4.36;
	}
	else if (tradeGoods == "tropical_wood")
	{
		tradeGoodsPrice = 2.52;
	}
	else if (tradeGoods == "silk")
	{
		tradeGoodsPrice = 4.47;
	}
	else if (tradeGoods == "incense")
	{
		tradeGoodsPrice = 2.74;
	}
	else if (tradeGoods == "livestock")
	{
		tradeGoodsPrice = 2.94;
	}
	else if (tradeGoods == "glass")
	{
		tradeGoodsPrice = 3.23;
	}
	else if (tradeGoods == "gems")
	{
		tradeGoodsPrice = 3.67;
	}
	else if (tradeGoods == "paper")
	{
		tradeGoodsPrice = 4.42;
	}
	else if (tradeGoods == "coal")
	{
		tradeGoodsPrice = 6;
	}
	else
	{
		// anything ive missed
		tradeGoodsPrice = 1;
	}

	return tradeGoodsPrice;
}


EU4::BuildingWeightEffects EU4::Province::getProvBuildingWeight(
	const Buildings& buildingTypes,
	const Modifiers& modifierTypes
) const
{
	BuildingWeightEffects effects;

	if (buildings)
	{
		for (auto buildingName : buildings->getBuildings())
		{
			auto theBuilding = buildingTypes.getBuilding(buildingName);
			
			if (theBuilding)
			{
				//effects.buildingWeight += theBuilding->getCost() * BUILDING_COST_TO_WEIGHT_RATIO; //double dipping
				if (theBuilding->isManufactory())
				{
					effects.manufactoriesValue += 0.9;
					effects.tradeGoodsSizeModifier += 0.1;
				}
				
				for (auto effect: theBuilding->getModifier().getAllEffects())
				{
					if (effect.first == "local_defensiveness")
					{
						effects.buildingWeight += (effect.second*4);
					}
					if (effect.first == "fort_level")
					{
						effects.buildingWeight += (effect.second*2);
					}
					if (effect.first == "local_state_maintenance_modifier")
					{
						effects.tradeSteering += (effect.second*-0.3);
					}
					if (effect.first == "local_development_cost")
					{
						effects.buildingWeight += (effect.second*-8);
					}
					if (effect.first == "naval_forcelimit")
					{
						effects.buildingWeight += (effect.second*3);
					}
					if (effect.first == "land_forcelimit")
					{
						effects.buildingWeight += (effect.second*6);
					}
					if (effect.first == "local_sailors_modifier")
					{
						effects.tradeSteering += (effect.second/8);
					}
					if (effect.first == "local_manpower_modifier")
					{
						effects.manpowerModifier += effect.second;
					}
					else if (effect.first == "local_tax_modifier")
					{
						effects.taxModifier += effect.second;
					}
					else if (effect.first == "tax_income")
					{
						effects.taxEfficiency += effect.second;
					}
					else if (effect.first == "local_production_efficiency")
					{
						effects.productionEfficiency += effect.second;
					}
					else if (effect.first == "trade_efficiency")  //not used
					{
						effects.tradeEfficiency += effect.second;
					}
					else if (effect.first == "trade_goods_size")
					{
						effects.manufactoriesValue += effect.second;
					}
					else if (effect.first == "trade_goods_size_modifier")
					{
						effects.tradeGoodsSizeModifier += effect.second;
					}
					else if (effect.first == "trade_value")
					{
						effects.tradeValue += effect.second;
					}
					else if (effect.first == "trade_value_modifier")
					{
						effects.tradeEfficiency += effect.second;
					}
					else if (effect.first == "province_trade_power_modifier")
					{
						effects.tradeSteering += (effect.second*0.15);
					}
					else if (effect.first == "province_trade_power_value")
					{
						effects.buildingWeight += (effect.second*0.75);
					}
					else if (effect.first == "local_missionary_strength")
					{
						effects.tradeSteering += (effect.second*1.66);
					}
				}
			}
			else
			{
				LOG(LogLevel::Warning) << "Could not look up information for building type " << buildingName;
			}
		}
	}

	for (auto modifierName: modifiers)
	{
		auto theModifier = modifierTypes.getModifier(modifierName);
		if (theModifier)
		{
			for (auto effect : theModifier->getAllEffects())
			{
					if (effect.first == "local_defensiveness")
					{
						effects.buildingWeight += (effect.second*4);
					}
					if (effect.first == "fort_level")
					{
						effects.buildingWeight += (effect.second*2);
					}
					if (effect.first == "local_state_maintenance_modifier")
					{
						effects.tradeSteering += (effect.second*-0.3);
					}
					if (effect.first == "local_development_cost")
					{
						effects.buildingWeight += (effect.second*-8);
					}
					if (effect.first == "naval_forcelimit")
					{
						effects.buildingWeight += (effect.second*3);
					}
					if (effect.first == "land_forcelimit")
					{
						effects.buildingWeight += (effect.second*6);
					}
					if (effect.first == "local_sailors_modifier")
					{
						effects.tradeSteering += (effect.second/8);
					}
					if (effect.first == "local_manpower_modifier")
					{
						effects.manpowerModifier += effect.second;
					}
					else if (effect.first == "local_tax_modifier")
					{
						effects.taxModifier += effect.second;
					}
					else if (effect.first == "tax_income")
					{
						effects.taxEfficiency += effect.second;
					}
					else if (effect.first == "local_production_efficiency")
					{
						effects.productionEfficiency += effect.second;
					}
					else if (effect.first == "trade_efficiency")  //not used
					{
						effects.tradeEfficiency += effect.second;
					}
					else if (effect.first == "trade_goods_size")
					{
						effects.manufactoriesValue += effect.second;
					}
					else if (effect.first == "trade_goods_size_modifier")
					{
						effects.tradeGoodsSizeModifier += effect.second;
					}
					else if (effect.first == "trade_value")
					{
						effects.tradeValue += effect.second;
					}
					else if (effect.first == "trade_value_modifier")
					{
						effects.tradeEfficiency += effect.second;
					}
					else if (effect.first == "province_trade_power_modifier")
					{
						effects.tradeSteering += (effect.second*0.15);
					}
					else if (effect.first == "province_trade_power_value")
					{
						effects.buildingWeight += (effect.second*0.75);
					}
					else if (effect.first == "local_missionary_strength")
					{
						effects.tradeSteering += (effect.second*1.66);
					}
			}
		}
		else
		{
			LOG(LogLevel::Warning) << "Could not look up information for modifier type " << modifierName;
		}
	}

	if (centerOfTradeLevel == 1)
	{
		effects.tradeSteering += 0.015;
		effects.buildingWeight += 3;
	}
	else if (centerOfTradeLevel == 2)
	{
		effects.tradeSteering += 0.03;
		effects.buildingWeight += 5.65;
	}
	else if (centerOfTradeLevel == 3)
	{
		effects.tradeSteering += 0.3575;
		effects.buildingWeight += 14.9;
	}

	return effects;
}
