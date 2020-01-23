/*Copyright (c) 2018 The Paradox Game Converters Project

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/



#include "V2Reforms.h"
#include "Log.h"
#include "../Configuration.h"
#include "../EU4World/EU4Country.h"
#include "V2Country.h"
#include "V2State.h"
#include "V2Province.h"



V2Reforms::V2Reforms(const V2Country* dstCountry, const std::shared_ptr<EU4::Country> srcCountry)
{
	if ((srcCountry->hasModifier("the_abolish_slavery_act")) || (srcCountry->hasModifier("abolish_slavery_act")))
	{
		abolishSlavery = true;
	}
	slavery = dstCountry->getSlavery();
	upper_house_composition = dstCountry->getUpper_house_composition();
	vote_franchise = dstCountry->getVote_franchise();
	voting_system = dstCountry->getVoting_system();
	public_meetings = dstCountry->getPublic_meetings();
	press_rights = dstCountry->getPress_rights();
	trade_unions = dstCountry->getTrade_unions();
	political_parties = dstCountry->getPolitical_parties();
}


void V2Reforms::output(FILE* output) const
{
	fprintf(output, "\n");
	fprintf(output, "# political reforms\n");
	fprintf(output, "slavery=yes_slavery\n");
	fprintf(output, "vote_franschise=none_voting\n");
	fprintf(output, "upper_house_composition=appointed\n");
	fprintf(output, "voting_system=first_past_the_post\n");
	fprintf(output, "public_meetings=no_meeting\n");
	fprintf(output, "press_rights=censored_press\n");
	fprintf(output, "trade_unions=state_controlled\n");
	fprintf(output, "political_parties=harassment\n");
}
