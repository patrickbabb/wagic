#include "PrecompiledHeader.h"

#include "MTGAbility.h"
#include "ManaCost.h"
#include "MTGGameZones.h"
#include "AllAbilities.h"
#include "Damage.h"
#include "TargetChooser.h"
#include "CardGui.h"
#include "MTGDeck.h"
#include "Translate.h"
#include "ThisDescriptor.h"
#include "ExtraCost.h"
#include "MTGRules.h"
#include "AbilityParser.h"


//Used for Lord/This parsing
const string kLordKeywords[] = { "lord(", "foreach(", "aslongas(", "teach(", "all(" };
const size_t kLordKeywordsCount = 5;

const string kThisKeywords[] = { "this(", "thisforeach(","while(", };
const size_t kThisKeywordsCount = 3;


// Used for the maxCast/maxPlay ability parsing
const string kMaxCastKeywords[] = { "maxplay(", "maxcast("};
const int kMaxCastZones[] = { MTGGameZone::BATTLEFIELD, MTGGameZone::STACK};
const size_t kMaxCastKeywordsCount = 2;

//Used for alternate Costs parsing
const string kAlternateCostKeywords[] = 
{ 
    "notpaid",
    "paidmana",
    "kicker", 
    "alternative", 
    "buyback", 
    "flashback", 
    "retrace", 
    "facedown",
    "suspended",
    "overload",
    "bestow"
}; 
const int kAlternateCostIds[] = 
{
    ManaCost::MANA_UNPAID,
    ManaCost::MANA_PAID,
    ManaCost::MANA_PAID_WITH_KICKER,
    ManaCost::MANA_PAID_WITH_ALTERNATIVE,
    ManaCost::MANA_PAID_WITH_BUYBACK, 
    ManaCost::MANA_PAID_WITH_FLASHBACK,
    ManaCost::MANA_PAID_WITH_RETRACE,
    ManaCost::MANA_PAID_WITH_MORPH,
    ManaCost::MANA_PAID_WITH_SUSPEND,
    ManaCost::MANA_PAID_WITH_OVERLOAD,
    ManaCost::MANA_PAID_WITH_BESTOW
};

//Used for "dynamic ability" parsing
const string kDynamicSourceKeywords[] = {"source", "mytgt", "myself", "myfoe"};
const int kDynamicSourceIds[] = { AADynamic::DYNAMIC_SOURCE_AMOUNT, AADynamic::DYNAMIC_MYTGT_AMOUNT, AADynamic::DYNAMIC_MYSELF_AMOUNT, AADynamic::DYNAMIC_MYFOE_AMOUNT};

const string kDynamicTypeKeywords[] = {"power", "toughness", "manacost", "colors", "age", "charge", "oneonecounters", "thatmuch"};
const int kDynamicTypeIds[] = {
    AADynamic::DYNAMIC_ABILITY_TYPE_POWER, AADynamic::DYNAMIC_ABILITY_TYPE_TOUGHNESS, AADynamic::DYNAMIC_ABILITY_TYPE_MANACOST, 
    AADynamic::DYNAMIC_ABILITY_TYPE_COLORS,  AADynamic::DYNAMIC_ABILITY_TYPE_AGE, AADynamic::DYNAMIC_ABILITY_TYPE_CHARGE, AADynamic::DYNAMIC_ABILITY_TYPE_ONEONECOUNTERS,
    AADynamic::DYNAMIC_ABILITY_TYPE_THATMUCH
};

const string kDynamicEffectKeywords[] = {"strike", "draw", "lifeloss", "lifegain", "pumppow", "pumptough", "pumpboth", "deplete", "countersoneone" };
const int kDynamicEffectIds[] = { 
    AADynamic::DYNAMIC_ABILITY_EFFECT_STRIKE, AADynamic::DYNAMIC_ABILITY_EFFECT_DRAW, AADynamic::DYNAMIC_ABILITY_EFFECT_LIFELOSS, AADynamic::DYNAMIC_ABILITY_EFFECT_LIFEGAIN,
    AADynamic::DYNAMIC_ABILITY_EFFECT_PUMPPOWER, AADynamic::DYNAMIC_ABILITY_EFFECT_PUMPTOUGHNESS,  AADynamic::DYNAMIC_ABILITY_EFFECT_PUMPBOTH, AADynamic::DYNAMIC_ABILITY_EFFECT_DEPLETE,
    AADynamic::DYNAMIC_ABILITY_EFFECT_COUNTERSONEONE
};


const string kDynamicWhoKeywords[] = {"eachother", "itself", "targetcontroller", "targetopponent", "tosrc", "srccontroller", "srcopponent" , "abilitycontroller" };
const int kDynamicWhoIds[] = { 
     AADynamic::DYNAMIC_ABILITY_WHO_EACHOTHER, AADynamic::DYNAMIC_ABILITY_WHO_ITSELF, AADynamic::DYNAMIC_ABILITY_WHO_TARGETCONTROLLER, AADynamic::DYNAMIC_ABILITY_WHO_TARGETOPPONENT,
     AADynamic::DYNAMIC_ABILITY_WHO_TOSOURCE,  AADynamic::DYNAMIC_ABILITY_WHO_SOURCECONTROLLER,  AADynamic::DYNAMIC_ABILITY_WHO_SOURCEOPPONENT, AADynamic::DYNAMIC_ABILITY_WHO_ABILITYCONTROLLER
};

int MTGAbility::allowedToCast(MTGCardInstance * card,Player * player)
{
    AbilityFactory af(game);
    return af.parseCastRestrictions(card,player,card->getRestrictions());
}

int MTGAbility::allowedToAltCast(MTGCardInstance * card,Player * player)
{
    AbilityFactory af(game);
    return af.parseCastRestrictions(card,player,card->getOtherRestrictions());
}

int AbilityFactory::parseCastRestrictions(MTGCardInstance * card, Player * player,string restrictions)
{
    vector <string> restriction = split(restrictions, ',');
    AbilityFactory af(observer);
    int cPhase = observer->getCurrentGamePhase();
    for(unsigned int i = 0;i < restriction.size();i++)
    {
        int checkPhaseBased = parseRestriction(restriction[i]);
        switch (checkPhaseBased)
        {
        case MTGAbility::PLAYER_TURN_ONLY:
            if (player != observer->currentPlayer)
                return 0;
            break;
        case MTGAbility::OPPONENT_TURN_ONLY:
            if (player == observer->currentPlayer)
                return 0;
            break;
        case MTGAbility::AS_SORCERY:
            if (player != observer->currentPlayer)
                return 0;
            if (cPhase != MTG_PHASE_FIRSTMAIN && cPhase != MTG_PHASE_SECONDMAIN)
                return 0;
            break;
        }
        if (checkPhaseBased >= MTGAbility::MY_BEFORE_BEGIN && checkPhaseBased <= MTGAbility::MY_AFTER_EOT)
        {
            if (player != observer->currentPlayer)
                return 0;
            if (cPhase != checkPhaseBased - MTGAbility::MY_BEFORE_BEGIN + MTG_PHASE_BEFORE_BEGIN)
                return 0;
        }
        if (checkPhaseBased >= MTGAbility::OPPONENT_BEFORE_BEGIN && checkPhaseBased <= MTGAbility::OPPONENT_AFTER_EOT)
        {
            if (player == observer->currentPlayer)
                return 0;
            if (cPhase != checkPhaseBased - MTGAbility::OPPONENT_BEFORE_BEGIN + MTG_PHASE_BEFORE_BEGIN)
                return 0;
        }
        if (checkPhaseBased >= MTGAbility::BEFORE_BEGIN && checkPhaseBased <= MTGAbility::AFTER_EOT)
        {
            if (cPhase != checkPhaseBased - MTGAbility::BEFORE_BEGIN + MTG_PHASE_BEFORE_BEGIN)
                return 0;
        }
        
        size_t typeRelated = restriction[i].find("type(");
        size_t seenType = restriction[i].find("lastturn(");
        size_t seenRelated = restriction[i].find("lastturn(");
        if(seenRelated == string::npos)
            seenRelated = restriction[i].find("thisturn(");
        size_t compRelated = restriction[i].find("compare(");

        size_t check = 0;
        if(typeRelated != string::npos || seenRelated != string::npos || compRelated != string::npos)
        {
            int firstAmount = 0;
            int secondAmount = 0;
            int mod=0;
            string rtc;
            vector<string> comparasion = split(restriction[i],'~');
            if(comparasion.size() != 3)
                return 0;//it was incorrectly coded, user should proofread card code.
            bool less = comparasion[1].find("lessthan") != string::npos;
            bool more = comparasion[1].find("morethan") != string::npos;
            bool equal = comparasion[1].find("equalto") != string::npos;
            for(unsigned int i = 0; i < comparasion.size(); i++)
            {
                check = comparasion[i].find("type(");
                if(check == string::npos)
                    check = comparasion[i].find("lastturn(");
                if(check == string::npos)
                    check = comparasion[i].find("thisturn(");
                if(check == string::npos)
                    check = comparasion[i].find("compare(");
                if( check != string::npos)
                {
                    size_t end = 0;
                    size_t foundType = comparasion[i].find("type(");
                    size_t foundComp = comparasion[i].find("compare(");
                    size_t foundSeen = comparasion[i].find("lastturn(");
                    if(foundSeen == string::npos)
                        foundSeen = comparasion[i].find("thisturn(");
                    if (foundType != string::npos)
                    {
                        end = comparasion[i].find(")", foundType);
                        rtc = comparasion[i].substr(foundType + 5, end - foundType - 5).c_str();
                        TargetChooserFactory tcf(observer);
                        TargetChooser * ttc = tcf.createTargetChooser(rtc,card);
                        mod = atoi(comparasion[i].substr(end+1).c_str());
                        bool withoutProtections = true;
                        if(i == 2)
                        {
                            secondAmount = ttc->countValidTargets(withoutProtections);
                            secondAmount += mod;
                        }
                        else
                        {
                            firstAmount = ttc->countValidTargets(withoutProtections);
                            firstAmount += mod;
                        }

                        SAFE_DELETE(ttc);
                    }
                    if (foundComp != string::npos)
                    {
                        end = comparasion[i].find(")", foundComp);
                        rtc = comparasion[i].substr(foundComp + 8, end - foundComp - 8).c_str();
                        mod = atoi(comparasion[i].substr(end+1).c_str());
                        if(i == 2)
                        {
                            WParsedInt * newAmount = NEW WParsedInt(rtc,card);
                            secondAmount = newAmount->getValue();
                            secondAmount += mod;
                            SAFE_DELETE(newAmount);
                        }
                        else
                        {
                            WParsedInt * newAmount = NEW WParsedInt(rtc,card);
                            firstAmount = newAmount->getValue();
                            firstAmount += mod;
                            SAFE_DELETE(newAmount);
                        }
                    }
                    if (foundSeen != string::npos)
                    {
                        end = comparasion[i].find(")", foundSeen);
                        rtc = comparasion[i].substr(foundSeen + 9, end - foundSeen - 9).c_str();
                        mod = atoi(comparasion[i].substr(end+1).c_str());

                        TargetChooserFactory tcf(observer);
                        TargetChooser * stc = tcf.createTargetChooser(rtc,card);
                        for (int w = 0; w < 2; ++w)
                        {
                            Player *p = observer->players[w];
                            MTGGameZone * zones[] = { p->game->inPlay, p->game->graveyard, p->game->hand, p->game->library, p->game->stack, p->game->exile, p->game->commandzone, p->game->sideboard, p->game->reveal };
                            for (int k = 0; k < 9; k++)
                            {
                                MTGGameZone * z = zones[k];
                                if (stc->targetsZone(z))
                                {
                                    if(i == 2)
                                    {
                                        secondAmount += seenType != string::npos ? z->seenLastTurn(rtc,Constants::CAST_ALL):z->seenThisTurn(rtc,Constants::CAST_ALL);

                                    }
                                    else
                                    {
                                        firstAmount += seenType != string::npos ? z->seenLastTurn(rtc,Constants::CAST_ALL):z->seenThisTurn(rtc,Constants::CAST_ALL);

                                    }
                                }
                            }
                        }
                        i == 2 ? secondAmount += mod:firstAmount += mod;
                        SAFE_DELETE(stc);
                    }
                }
                else if (i == 2)
                {
                    WParsedInt * secondA = NEW WParsedInt(comparasion[2].c_str(),NULL,card);
                    secondAmount = secondA->getValue();
                    SAFE_DELETE(secondA);
                }
            }
            if(firstAmount < secondAmount && !less && !more && !equal)
                return 0;
            if(equal && firstAmount != secondAmount)
                return 0;
            if(less && firstAmount >= secondAmount)
                return 0;
            if(more  && firstAmount <= secondAmount)
                return 0;
        }
        check = restriction[i].find("turn:");
        if(check != string::npos)
        {
            int Turn = 0;
            size_t start = restriction[i].find(":", check);
            size_t end = restriction[i].find(" ", check);
            WParsedInt* parser = NEW WParsedInt(restriction[i].substr(start + 1, end - start - 1), card);
            if(parser){
                Turn = parser->intValue;
                SAFE_DELETE(parser);
            }
            if(observer->turn < Turn-1)
                return 0;
        }
        check = restriction[i].find("casted a spell");
        if(check != string::npos)
        {
            if(player->game->stack->seenThisTurn("*", Constants::CAST_ALL) < 1)
                return 0;
        }
        check = restriction[i].find("casted(");
        if(check != string::npos)
        {
            size_t end = restriction[i].find(")",check);
            string tc = restriction[i].substr(check + 7,end - check - 7);
            if(tc.find("|mystack") != string::npos)
            {
                if(player->game->stack->seenThisTurn(tc, Constants::CAST_ALL) < 1)
                    return 0;
            }
            else if(tc.find("|opponentstack") != string::npos)
            {
                if(player->opponent()->game->stack->seenThisTurn(tc, Constants::CAST_ALL) < 1)
                    return 0;
            }
            else if(tc.find("this") != string::npos)
            {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card)
                        count++;
                    if(stackCard == card)
                        count++;
                }
                if(!count)
                    return 0;
            }
        }
        check = restriction[i].find("gravecast");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card && (card->previousZone == card->controller()->game->graveyard||card->previousZone == card->controller()->opponent()->game->graveyard))
                        count++;
                    if(stackCard == card && (card->previousZone == card->controller()->game->graveyard||card->previousZone == card->controller()->opponent()->game->graveyard))
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("librarycast");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card && (card->previousZone == card->controller()->game->library||card->previousZone == card->controller()->opponent()->game->library))
                        count++;
                    if(stackCard == card && (card->previousZone == card->controller()->game->library||card->previousZone == card->controller()->opponent()->game->library))
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("exilecast");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card && (card->previousZone == card->controller()->game->exile||card->previousZone == card->controller()->opponent()->game->exile))
                        count++;
                    if(stackCard == card && (card->previousZone == card->controller()->game->exile||card->previousZone == card->controller()->opponent()->game->exile))
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("sidecast");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card && (card->previousZone == card->controller()->game->sideboard||card->previousZone == card->controller()->opponent()->game->sideboard))
                        count++;
                    if(stackCard == card && (card->previousZone == card->controller()->game->sideboard||card->previousZone == card->controller()->opponent()->game->sideboard))
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("commandzonecast");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card && (card->previousZone == card->controller()->game->commandzone||card->previousZone == card->controller()->opponent()->game->commandzone))
                        count++;
                    if(stackCard == card && (card->previousZone == card->controller()->game->commandzone||card->previousZone == card->controller()->opponent()->game->commandzone))
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("rebound");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->stack->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * stackCard = player->game->stack->cardsSeenThisTurn[k];
                    if(stackCard->next && stackCard->next == card && (card->previousZone == card->controller()->game->hand||card->previousZone == card->controller()->opponent()->game->hand))
                        count++;
                    if(stackCard == card && (card->previousZone == card->controller()->game->hand||card->previousZone == card->controller()->opponent()->game->hand))
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("revolt");
        if(check != string::npos)
        {
                int count = 0;
                for(unsigned int k = 0; k < player->game->hand->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * tCard = player->game->hand->cardsSeenThisTurn[k];
                    if(tCard && tCard->previousZone == card->controller()->game->battlefield)
                        count++;
                }
                for(unsigned int k = 0; k < player->game->exile->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * tCard = player->game->exile->cardsSeenThisTurn[k];
                    if(tCard && tCard->previousZone == card->controller()->game->battlefield)
                        count++;
                }
                for(unsigned int k = 0; k < player->game->library->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * tCard = player->game->library->cardsSeenThisTurn[k];
                    if(tCard && tCard->previousZone == card->controller()->game->battlefield)
                        count++;
                }
                for(unsigned int k = 0; k < player->game->graveyard->cardsSeenThisTurn.size(); k++)
                {
                    MTGCardInstance * tCard = player->game->graveyard->cardsSeenThisTurn[k];
                    if(tCard && tCard->previousZone == card->controller()->game->battlefield)
                        count++;
                }
                if(!count)
                    return 0;
        }
        check = restriction[i].find("morbid");
        if(check != string::npos)
        {
            bool isMorbid = false;
            for(int cp = 0;cp < 2;cp++)
            {
                Player * checkCurrent = observer->players[cp];
                MTGGameZone * grave = checkCurrent->game->graveyard;
                for(unsigned int gy = 0;gy < grave->cardsSeenThisTurn.size();gy++)
                {
                    MTGCardInstance * checkCard = grave->cardsSeenThisTurn[gy];
                    if(checkCard->isCreature() &&
                        ((checkCard->previousZone == checkCurrent->game->battlefield)||
                        (checkCard->previousZone == checkCurrent->opponent()->game->battlefield))//died from battlefield
                        )
                    {
                        isMorbid = true;
                        break;
                    }
                }
                if(isMorbid)
                    break;
            }
            if(!isMorbid)
                return 0;
        }
        check = restriction[i].find("deadpermanent");
        if(check != string::npos)
        {
            bool deadpermanent = false;
            for(int cp = 0;cp < 2;cp++)
            {
                Player * checkCurrent = observer->players[cp];
                MTGGameZone * grave = checkCurrent->game->graveyard;
                for(unsigned int gy = 0;gy < grave->cardsSeenThisTurn.size();gy++)
                {
                    MTGCardInstance * checkCard = grave->cardsSeenThisTurn[gy];
                    if(checkCard->isPermanent() &&
                        ((checkCard->previousZone == checkCurrent->game->battlefield)||
                        (checkCard->previousZone == checkCurrent->opponent()->game->battlefield))//died from battlefield
                        )
                    {
                        deadpermanent = true;
                        break;
                    }
                }
                if(deadpermanent)
                    break;
            }
            if(!deadpermanent)
                return 0;
        }
        check = restriction[i].find("deadcreart");
        if(check != string::npos)
        {
            bool deadcreart = false;
            for(int cp = 0;cp < 2;cp++)
            {
                Player * checkCurrent = observer->players[cp];
                MTGGameZone * grave = checkCurrent->game->graveyard;
                for(unsigned int gy = 0;gy < grave->cardsSeenThisTurn.size();gy++)
                {
                    MTGCardInstance * checkCard = grave->cardsSeenThisTurn[gy];
                    if((checkCard->isCreature() || checkCard->hasType(Subtypes::TYPE_ARTIFACT)) &&
                        ((checkCard->previousZone == checkCurrent->game->battlefield)||
                        (checkCard->previousZone == checkCurrent->opponent()->game->battlefield))//died from battlefield
                        )
                    {
                        deadcreart = true;
                        break;
                    }
                }
                if(deadcreart)
                    break;
            }
            if(!deadcreart)
                return 0;
        }
        check = restriction[i].find("hasdead");
        if(check != string::npos)
        {
            bool hasdeadtype = false;
            string checktype = "";
            Player * checkCurrent = NULL;
            if(restriction[i].find("oppo") != string::npos){
                checktype = restriction[i].substr(11);
                checkCurrent = card->controller()->opponent();
            } else {
                checktype = restriction[i].substr(9);
                checkCurrent = card->controller();
            }
            MTGGameZone * grave = checkCurrent->game->graveyard;
            for(unsigned int gy = 0; gy < grave->cardsSeenThisTurn.size(); gy++)
            {
                MTGCardInstance * checkCard = grave->cardsSeenThisTurn[gy];
                if(checkCard->hasType(checktype) &&
                    ((checkCard->previousZone == checkCurrent->game->battlefield))) //died from battlefield
                {
                    hasdeadtype = true;
                    break;
                }
            }
            if(!hasdeadtype)
                return 0;
        }
        check = restriction[i].find("zerodead");
        if(check != string::npos)//returns true if zero
        {
            bool hasDeadCreature = false;
            Player * checkCurrent = card->controller();
            MTGGameZone * grave = checkCurrent->game->graveyard;
            for(unsigned int gy = 0;gy < grave->cardsSeenThisTurn.size();gy++)
            {
                MTGCardInstance * checkCard = grave->cardsSeenThisTurn[gy];
                if(checkCard->isCreature() &&
                    ((checkCard->previousZone == checkCurrent->game->battlefield))//died from your battlefield
                    )
                {
                    hasDeadCreature = true;
                    break;
                }
            }
            if(hasDeadCreature)
                return 0;
        }
        //Ensnaring Bridge
        check = restriction[i].find("powermorethanopponenthand");
        if (check != string::npos)//for opponent creatures
        {
            Player * checkCurrent = card->controller();
            if(card->power <= checkCurrent->opponent()->game->hand->nb_cards)
                return 0;
        }

        check = restriction[i].find("powermorethancontrollerhand");
        if (check != string::npos)//for controller creatures
        {
            Player * checkCurrent = card->controller();
            if(card->power <= checkCurrent->game->hand->nb_cards)
                return 0;
        }
        //end

        check = restriction[i].find("morecardsthanopponent");
        if (check != string::npos)
        {
            Player * checkCurrent = card->controller();
            if(checkCurrent->game->hand->nb_cards <= checkCurrent->opponent()->game->hand->nb_cards)
                return 0;
        }

        check = restriction[i].find("delirium");
        if (check != string::npos)
        {
                Player * checkCurrent = card->controller();
                MTGGameZone * grave = checkCurrent->game->graveyard;

                int checkTypesAmount = 0;
                if (grave->hasType("creature")) checkTypesAmount++;
                if (grave->hasType("enchantment")) checkTypesAmount++;
                if (grave->hasType("sorcery")) checkTypesAmount++;
                if (grave->hasType("instant")) checkTypesAmount++;
                if (grave->hasType("land")) checkTypesAmount++;
                if (grave->hasType("artifact")) checkTypesAmount++;
                if (grave->hasType("planeswalker")) checkTypesAmount++;
                if (grave->hasType("battle")) checkTypesAmount++;
                if (grave->hasType("kindred")) checkTypesAmount++;
                if (checkTypesAmount < 4)
                return 0;
        }

        check = restriction[i].find("notdelirum");
        if (check != string::npos)
        {
                Player * checkCurrent = card->controller();
                MTGGameZone * grave = checkCurrent->game->graveyard;

                int checkTypesAmount = 0;
                if (grave->hasType("creature")) checkTypesAmount++;
                if (grave->hasType("enchantment")) checkTypesAmount++;
                if (grave->hasType("sorcery")) checkTypesAmount++;
                if (grave->hasType("instant")) checkTypesAmount++;
                if (grave->hasType("land")) checkTypesAmount++;
                if (grave->hasType("artifact")) checkTypesAmount++;
                if (grave->hasType("planeswalker")) checkTypesAmount++;
                if (grave->hasType("battle")) checkTypesAmount++;
                if (grave->hasType("kindred")) checkTypesAmount++;
                if (checkTypesAmount > 3)
                return 0;
        }

        check = restriction[i].find("miracle");
        if(check != string::npos)
        {
            if(observer->turn < 1 && card->controller()->drawCounter < 1)
                return 0;
            if(card->previous && !card->previous->miracle)
                return 0;
        }

        check = restriction[i].find("madnessplayed");
        if (check != string::npos)
        {
            if (card->previous && !card->previous->MadnessPlay)
                return 0;
        }

        check = restriction[i].find("prowl");
        if(check != string::npos)
        {
            vector<string>typeToCheck = parseBetween(restriction[i],"prowl(",")");
            bool isProwled = false;
            if(typeToCheck.size())
            {
                vector<string>splitTypes = split(typeToCheck[1],' ');
                if(splitTypes.size())
                {
                    for (size_t k = 0; k < splitTypes.size(); ++k)
                    {
                        string theType = splitTypes[k];
                        for (size_t j = 0; j < card->controller()->prowledTypes.size(); ++j)
                        {
                            if ( card->controller()->prowledTypes[j] == splitTypes[k])
                            {
                                isProwled = true;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    for (size_t j = 0; j < card->controller()->prowledTypes.size(); ++j)
                    {
                        if (  card->controller()->prowledTypes[j] == typeToCheck[1])
                        {
                            isProwled = true;
                            break;
                        }
                    }
                }

            }
            else
            {
                for (size_t j = 0; j < card->controller()->prowledTypes.size(); ++j)
                {
                    if ( card->hasSubtype( card->controller()->prowledTypes[j] ))
                    {
                        isProwled = true;
                        break;
                    }
                }
            }
            if(!isProwled)
                return 0;
        }

        check = restriction[i].find("spent(");
        if(check != string::npos)
        {
            vector<string>spentMana = parseBetween(restriction[i],"spent(",")");
            if(spentMana.size())
            {
                ManaCost * costToCheck = ManaCost::parseManaCost(restriction[i]);
                ManaCost * spent = card->getManaCost()->getManaUsedToCast();
                if(spent && costToCheck && !spent->canAfford(costToCheck,0))
                {
                    SAFE_DELETE(costToCheck);
                    return 0;
                }
                SAFE_DELETE(costToCheck);
            }
        }

        check = restriction[i].find("discarded");
        if(check != string::npos)
        {
            if(!card->discarded)
                return 0;
        }

        check = restriction[i].find("hasexerted");
        if(check != string::npos)
        {
            if(!card->exerted)
                return 0;
        }

        check = restriction[i].find("notexerted");
        if(check != string::npos)
        {
            if(card->exerted)
                return 0;
        }

        check = restriction[i].find("discardbyopponent");
        if(check != string::npos)
        {
            bool matchOpponent = false;
            if(card->discarderOwner)
                if(card->controller()->opponent() == card->discarderOwner)
                    matchOpponent = true;

            if(!matchOpponent)
                return 0;
        }

        check = restriction[i].find("copiedacard");
        if(check != string::npos)
        {
            if(!card->isACopier)
                return 0;
        }

        check = restriction[i].find("geared");
        if (check != string::npos)
        {
            if (card->equipment < 1)
                return 0;
        }

        check = restriction[i].find("canuntap");
        if(check != string::npos)
        {
            if(card->frozen >= 1 || card->basicAbilities[(int)Constants::DOESNOTUNTAP] || !card->isTapped())
                return 0;
        }

        check = restriction[i].find("raid");
        if(check != string::npos)
        {
            if(card->controller()->raidcount < 1)
                return 0;
        }
        
        check = restriction[i].find("oppoattacked");
        if(check != string::npos)
        {
            if(card->controller()->opponent()->raidcount < 1)
                return 0;
        }

        check = restriction[i].find("opponentdamagedbycombat");
        if(check != string::npos)
        {
            if(card->controller()->dealsdamagebycombat < 1)
                return 0;
        }

        check = restriction[i].find("lessorequalcreatures");
        if(check != string::npos)
        {
            bool condition = (card->controller()->opponent()->inPlay()->countByType("creature") >= card->controller()->inPlay()->countByType("creature"));
            if(!condition)
                return 0;
        }

        check = restriction[i].find("lessorequallands");
        if(check != string::npos)
        {
            bool condition = (card->controller()->opponent()->inPlay()->countByType("land") >= card->controller()->inPlay()->countByType("land"));
            if(!condition)
                return 0;
        }

        check = restriction[i].find("outnumbered");//opponent controls atleast 4 or more creatures than you
        if(check != string::npos)
        {
            bool isoutnumbered = (card->controller()->opponent()->inPlay()->countByType("creature") - card->controller()->inPlay()->countByType("creature"))>3;
            if(!isoutnumbered)
                return 0;
        }

        check = restriction[i].find("hasdefender");
        if(check != string::npos)
        {
            if(!card->has(Constants::DEFENDER))
                return 0;
        }

        check = restriction[i].find("didblock");
        if(check != string::npos)
        {
            if(!card->didblocked)
                return 0;
        }

        check = restriction[i].find("didattack");
        if(check != string::npos)
        {
            if(!card->didattacked)
                return 0;
        }

        check = restriction[i].find("didntattack");
        if(check != string::npos)
        {
            if(card->didattacked)
                return 0;
        }
        
        check = restriction[i].find("didcombatdamagetofoe");
        if(check != string::npos)
        {
            if(card->combatdamageToOpponent == 0)
                return 0;
        }

        check = restriction[i].find("ownerscontrol");
        if(check != string::npos)
        {
            if(card->currentZone != card->owner->game->battlefield)
                return 0;
        }
        check = restriction[i].find("opponentscontrol");
        if(check != string::npos)
        {
            if(card->currentZone == card->owner->game->battlefield)
                return 0;
        }
        check = restriction[i].find("one of a kind");
        if(check != string::npos)
        {
            if(player->game->inPlay->hasName(card->name))
                return 0;
        }
        check = restriction[i].find("before attackers");
        if(check != string::npos)
        {
            if(cPhase > MTG_PHASE_COMBATBEGIN)
                return 0;
        }
        check = restriction[i].find("before battle damage");
        if(check != string::npos)
        {
            if(cPhase > MTG_PHASE_COMBATBLOCKERS)
                return 0;
        }
        check = restriction[i].find("after battle");
        if(check != string::npos)
        {
            if(cPhase < MTG_PHASE_COMBATBLOCKERS)
                return 0;
        }
        check = restriction[i].find("during battle");
        if(check != string::npos)
        {
            if(cPhase < MTG_PHASE_COMBATBEGIN ||cPhase > MTG_PHASE_COMBATEND )
                return 0;
        }
        check = restriction[i].find("during my main phases");
        if(check != string::npos)
        {
            if( player != observer->currentPlayer && (cPhase != MTG_PHASE_FIRSTMAIN ||cPhase != MTG_PHASE_SECONDMAIN) )
                return 0;
        }
        check = restriction[i].find("during my turn");
        if(check != string::npos)
        {
            if(player != observer->currentPlayer)
                return 0;
        }
        check = restriction[i].find("during opponent turn");
        if(check != string::npos)
        {
            if(player == observer->currentPlayer)
                return 0;
        }
        check = restriction[i].find("control snow land");
        if(check != string::npos)
        {
            if(!player->game->inPlay->hasPrimaryType("snow","land"))
                return 0;
        }
        check = restriction[i].find("control two or more vampires");
        if(check != string::npos)
        {
            restriction.push_back("type(vampire|mybattlefield)~morethan~1");
        }
        check = restriction[i].find("control less artifacts");
        if(check != string::npos)
        {
            restriction.push_back("type(artifact|mybattlefield)~lessthan~type(artifact|opponentbattlefield)");
        }
        check = restriction[i].find("control less enchantments");
        if(check != string::npos)
        {
            restriction.push_back("type(enchantment|mybattlefield)~lessthan~type(enchantment|opponentbattlefield)");
        }
        check = restriction[i].find("control less creatures");
        if(check != string::npos)
        {
            restriction.push_back("type(creature|mybattlefield)~lessthan~type(creature|opponentbattlefield)");
        }
        check = restriction[i].find("control less lands");
        if(check != string::npos)
        {
            restriction.push_back("type(land|mybattlefield)~lessthan~type(land|opponentbattlefield)");
        }
        check = restriction[i].find("control more creatures");
        if(check != string::npos)
        {
            restriction.push_back("type(creature|mybattlefield)~morethan~type(creature|opponentbattlefield)");
        }
        check = restriction[i].find("control more lands");
        if(check != string::npos)
        {
            restriction.push_back("type(land|mybattlefield)~morethan~type(land|opponentbattlefield)");
        }
        check = restriction[i].find("didnotcastnontoken");
        if(check != string::npos)
        {
            restriction.push_back("lastturn(*[-token]|opponentstack,opponentbattlefield)~lessthan~1");
        }
        check = restriction[i].find("control three or more lands with same name");
        if(check != string::npos)
        {
            if(player != observer->currentPlayer)
                return 0;
            bool found = false;
            for(unsigned int i = 0; i < observer->currentPlayer->game->inPlay->cards.size() && !found; i++){
                if(observer->currentPlayer->game->inPlay->cards[i]->hasType("land")){
                    for(unsigned int j = i+1; j < observer->currentPlayer->game->inPlay->cards.size() && !found; j++){
                        if(observer->currentPlayer->game->inPlay->cards[j]->hasType("land") && observer->currentPlayer->game->inPlay->cards[j]->name == observer->currentPlayer->game->inPlay->cards[i]->name){
                            for(unsigned int k = j+1; k < observer->currentPlayer->game->inPlay->cards.size() && !found; k++){
                                if(observer->currentPlayer->game->inPlay->cards[k]->hasType("land") && observer->currentPlayer->game->inPlay->cards[k]->name == observer->currentPlayer->game->inPlay->cards[i]->name){
                                    found = true;
                                }
                            }
                        }
                    }
                }
            }
            if(!found)
                return 0;
        }
        check = restriction[i].find("coven"); //Player controls three or more creatures with different powers
        if(check != string::npos)
        {
            if(player != observer->currentPlayer)
                return 0;
            bool found = false;
            for(unsigned int i = 0; i < observer->currentPlayer->game->inPlay->cards.size() && !found; i++){
                if(observer->currentPlayer->game->inPlay->cards[i]->hasType(Subtypes::TYPE_CREATURE)){
                    for(unsigned int j = i+1; j < observer->currentPlayer->game->inPlay->cards.size() && !found; j++){
                        if(observer->currentPlayer->game->inPlay->cards[j]->hasType(Subtypes::TYPE_CREATURE) && observer->currentPlayer->game->inPlay->cards[j]->power != observer->currentPlayer->game->inPlay->cards[i]->power){
                            for(unsigned int k = j+1; k < observer->currentPlayer->game->inPlay->cards.size() && !found; k++){
                                if(observer->currentPlayer->game->inPlay->cards[k]->hasType(Subtypes::TYPE_CREATURE) && (observer->currentPlayer->game->inPlay->cards[k]->power != observer->currentPlayer->game->inPlay->cards[i]->power && observer->currentPlayer->game->inPlay->cards[k]->power != observer->currentPlayer->game->inPlay->cards[j]->power)){
                                    found = true;
                                }
                            }
                        }
                    }
                }
            }
            if(!found)
                return 0;
        }
        check = restriction[i].find("trainer"); //Player controls an attacking creature with greater power than the current one.
        if(check != string::npos)
        {
            if(player != observer->currentPlayer || !card->isAttacker())
                return 0;
            bool found = false;
            for(unsigned int i = 0; i < observer->currentPlayer->game->inPlay->cards.size() && !found; i++){
                if(observer->currentPlayer->game->inPlay->cards[i]->hasType(Subtypes::TYPE_CREATURE) && observer->currentPlayer->game->inPlay->cards[i]->isAttacker() && observer->currentPlayer->game->inPlay->cards[i]->power > card->power){
                    found = true;
                }
            }
            if(!found)
                return 0;
        }
        check = restriction[i].find("can play");
        if(check != string::npos)
        {
            if(!card->getId())
                return 0; //Fixed a crash when AI plays.
            string type = restriction[i];
            type = type.replace(0,9,"");
            MTGCardInstance* cardDummy = card->clone();
            for (int i = ((int)cardDummy->types.size())-1; i >= 0; --i)
                cardDummy->removeType(cardDummy->types[i]);
            cardDummy->addType(type);
            bool canplay = true;
            if (cardDummy->isLand() && observer->currentActionPlayer->game->playRestrictions->canPutIntoZone(cardDummy, observer->currentActionPlayer->game->inPlay) == PlayRestriction::CANT_PLAY)
                canplay = false;
            if (!cardDummy->isLand() && observer->currentActionPlayer->game->playRestrictions->canPutIntoZone(cardDummy, observer->currentActionPlayer->game->stack) == PlayRestriction::CANT_PLAY)
                canplay = false;
            if ((cardDummy->owner == observer->currentActionPlayer) && !cardDummy->hasType(Subtypes::TYPE_INSTANT) && !cardDummy->StackIsEmptyandSorcerySpeed())
                canplay = false;
            SAFE_DELETE(cardDummy);
            if(!canplay)
                return 0;
        }
        check = restriction[i].find("paid(");
        if(check != string::npos)
        {
            vector<string>getPaid = parseBetween(restriction[i].c_str(),"paid(",")");
            string paid = getPaid[1];
            for (size_t j = 0; j < sizeof(kAlternateCostIds)/sizeof(kAlternateCostIds[0]); ++j)
            {
                string keyword = kAlternateCostKeywords[j];
                if (paid.find(keyword) != string::npos)
                {
                    if (!(card->alternateCostPaid[j] > 0 ))
                    {
                        return 0;
                    }
                }
            }
        }
        check = restriction[i].find("never");
        if(check != string::npos)
            return 0;
    }
    return 1;
}

int AbilityFactory::parseRestriction(string s)
{
    if (s.find("myturnonly") != string::npos)
        return ActivatedAbility::PLAYER_TURN_ONLY;
    if (s.find("opponentturnonly") != string::npos)
        return ActivatedAbility::OPPONENT_TURN_ONLY;
    if (s.find("assorcery") != string::npos)
        return ActivatedAbility::AS_SORCERY;

    string types[] = { "my", "opponent", "" };
    int starts[] = { ActivatedAbility::MY_BEFORE_BEGIN, ActivatedAbility::OPPONENT_BEFORE_BEGIN, ActivatedAbility::BEFORE_BEGIN };
    for (int j = 0; j < 3; ++j)
    {
        size_t found = s.find(types[j]);
        if (found != string::npos)
        {
            for (int i = 0; i < NB_MTG_PHASES; i++)
            {
                string toFind = types[j];
                toFind.append(Constants::MTGPhaseCodeNames[i]).append("only");
                found = s.find(toFind);
                if (found != string::npos)
                {
                    return starts[j] + i;
                }
            }
        }
    }

    return ActivatedAbility::NO_RESTRICTION;
}

int AbilityFactory::countCards(TargetChooser * tc, Player * player, int option)
{
    int result = 0;
    for (int i = 0; i < 2; i++)
    {
        if (player && player != observer->players[i])
            continue;
        MTGGameZone * zones[] = { observer->players[i]->game->inPlay, observer->players[i]->game->graveyard, observer->players[i]->game->hand, observer->players[i]->game->exile, observer->players[i]->game->commandzone };
        for (int k = 0; k < 5; k++)
        {
            for (int j = zones[k]->nb_cards - 1; j >= 0; j--)
            {
                MTGCardInstance * current = zones[k]->cards[j];
                if (tc->canTarget(current))
                {
                    switch (option)
                    {
                    case COUNT_POWER:
                        result += current->power;
                        break;
                    default:
                        result++;
                        break;
                    }
                }
            }
        }
    }
    return result;
}

Counter * AbilityFactory::parseCounter(string s, MTGCardInstance * target, Spell * spell)
{
    int nb = 1;
    int maxNb = 0;
    string name = "";
    string nbstr = "1";
    string maxNbstr = "0";
    string spt = "";

    vector<string>splitCounter = split(s,',');
    vector<string>splitCounterCheck = split(s,'.');
    if(splitCounter.size() < splitCounterCheck.size())
    {
        splitCounter = splitCounterCheck;//use the one with the most results.
    }
    if(!splitCounter.size())
    {
        return NULL;
    }
    if(splitCounter.size() > 0)//counter(1/1)
    {
        spt = splitCounter[0];
    }
    if(splitCounter.size() > 1)//counter(1/1,1)
    {
        nbstr = splitCounter[1];
    }
    if(splitCounter.size() > 2)//counter(0/0,1,charge)
    {
        name = splitCounter[2];
    }
    if(splitCounter.size() > 3)//counter(0/0,1,charge,2)
    {
        maxNbstr = splitCounter[3];
    }
    WParsedInt * wpi;
    if (target)
    {
        wpi = NEW WParsedInt(nbstr, spell, target);
    }
    else
    {
        wpi = NEW WParsedInt(atoi(nbstr.c_str()));
    }
    nb = wpi->getValue();
    delete (wpi);
    WParsedInt * wpinb;
    if (target)
    {
        wpinb = NEW WParsedInt(maxNbstr, spell, target);
    }
    else
    {
        wpinb = NEW WParsedInt(atoi(maxNbstr.c_str()));
    }
    maxNb = wpinb->getValue();
    delete(wpinb);

    int power, toughness;
    if (parsePowerToughness(spt, &power, &toughness))
    {
        Counter * counter = NEW Counter(target, name.c_str(), power, toughness);
        counter->nb = nb;
        counter->maxNb = maxNb;
        return counter;
    }
    return NULL;
}

int AbilityFactory::parsePowerToughness(string s, int *power, int *toughness)
{
    vector<string>splitPT = split(s,'/');
    vector<string>splitPTCheck = split(s,'%');
    if(splitPT.size() < 2 && splitPT.size() < splitPTCheck.size())
    {
        splitPT = splitPTCheck;
    }
    if(!splitPT.size())
    {
        return 0;
    }
    *power = atoi(splitPT[0].c_str());
    *toughness = atoi(splitPT[1].c_str());
    return 1;
}

TargetChooser * AbilityFactory::parseSimpleTC(const std::string& s, const std::string& _starter, MTGCardInstance * card, bool forceNoTarget)
{
    string starter = _starter;
    starter.append("(");

    size_t found = s.find(starter);
    if (found == string::npos)
        return NULL;

    size_t start = found + starter.size(); 

    size_t end = s.find(")", start);
    if (end == string::npos)
    {
        DebugTrace("malformed syntax " << s);
        return NULL;
    }

    string starget = s.substr(start , end - start);
    TargetChooserFactory tcf(observer);
    TargetChooser * tc =  tcf.createTargetChooser(starget, card);

    if (tc && forceNoTarget)
        tc->targetter = NULL;

    return tc;
}

// evaluate trigger ability
// ie auto=@attacking(mytgt):destroy target(*)
// eval only the text between the @ and the first :
TriggeredAbility * AbilityFactory::parseTrigger(string s, string, int id, Spell *, MTGCardInstance *card,
                                                Targetable * target)
{
    size_t found = string::npos;

    //restrictions on triggers  
    bool once = (s.find("once") != string::npos);
    bool sourceUntapped = (s.find("sourcenottap") != string::npos);
    bool sourceTap = (s.find("sourcetap") != string::npos);
    bool limitOnceATurn = (s.find("turnlimited") != string::npos);
    bool isSuspended = (s.find("suspended") != string::npos);
    bool opponentPoisoned = (s.find("opponentpoisoned") != string::npos);
    bool lifelost = (s.find("foelost(") != string::npos);
    int lifeamount = lifelost ? atoi(s.substr(s.find("foelost(") + 8,')').c_str()) : 0;
    bool neverRemove = (s.find("dontremove") != string::npos);

    //Card Changed Zone
    found = s.find("movedto(");
    if (found != string::npos)
    {
        size_t end = s.find(")");
        string starget = s.substr(found + 8, end - found - 8);
        TargetChooserFactory tcf(observer);

        TargetChooser *toTc = NULL;
        TargetChooser *toTcCard = NULL;
        end = starget.find("|");
        if (end == string::npos)
        {
            toTcCard = tcf.createTargetChooser("*", card);
            found = 0;
        }
        else
        {
            toTcCard = tcf.createTargetChooser(starget.substr(0, end).append("|*"), card);
            found = end + 1;
        }
        toTcCard->setAllZones();
        toTcCard->targetter = NULL; //avoid protection from
        starget = starget.substr(found, end - found).insert(0, "*|");
        toTc = tcf.createTargetChooser(starget, card);
        toTc->targetter = NULL; //avoid protection from

        TargetChooser *fromTc = NULL;
        TargetChooser * fromTcCard = NULL;
        found = s.find("from(");
        if (found != string::npos)
        {
            end = s.find("|", found);
            if (end == string::npos)
            {
                fromTcCard = tcf.createTargetChooser("*", card);
                found = found + 5;
            }
            else
            {
                fromTcCard = tcf.createTargetChooser(s.substr(found + 5, end - found - 5).append("|*"), card);
                found = end + 1;
            }
            fromTcCard->setAllZones();
            fromTcCard->targetter=NULL; //avoid protection from
            end = s.find(")", found);
            starget = s.substr(found, end - found).insert(0, "*|");
            fromTc = tcf.createTargetChooser(starget, card);
            fromTc->targetter = NULL; //avoid protection from
        }
        TriggeredAbility * mover = NEW TrCardAddedToZone(observer, id, card, (TargetZoneChooser *) toTc, toTcCard, (TargetZoneChooser *) fromTc, fromTcCard, once, sourceUntapped, isSuspended, limitOnceATurn);
        if(neverRemove && mover)
        {
            mover->forcedAlive = 1;
            mover->forceDestroy = -1;
        }
        return mover;
    }

    //Card unTapped
    if (TargetChooser *tc = parseSimpleTC(s,"untapped", card))
        return NEW TrCardTapped(observer, id, card, tc, false, once, limitOnceATurn);

    //Card Tapped
    if (TargetChooser *tc = parseSimpleTC(s,"tapped", card))
        return NEW TrCardTapped(observer, id, card, tc, true, once, limitOnceATurn);

    //Card Tapped for mana
    if (TargetChooser *tc = parseSimpleTC(s,"tappedformana", card))
        return NEW TrCardTappedformana(observer, id, card, tc, true, once, limitOnceATurn);

    //Card Produced some mana
    if (TargetChooser *tc = parseSimpleTC(s,"producedmana", card))
        return NEW TrCardManaproduced(observer, id, card, tc, once, limitOnceATurn);

    //Card Transforms
    if (TargetChooser *tc = parseSimpleTC(s,"transformed", card))
        return NEW TrCardTransformed(observer, id, card, tc, once, limitOnceATurn);

    //Card Faces Up
    if (TargetChooser *tc = parseSimpleTC(s,"facedup", card))
        return NEW TrCardFaceUp(observer, id, card, tc, once, limitOnceATurn);

    //Card Phases In
    if (TargetChooser *tc = parseSimpleTC(s,"phasedin", card))
        return NEW TrCardPhasesIn(observer, id, card, tc, once, limitOnceATurn);

    //Card Exerted
    if (TargetChooser *tc = parseSimpleTC(s,"exerted", card))
        return NEW TrCardExerted(observer, id, card, tc, once, limitOnceATurn);

//CombatTrigger
    //Card attacked and is blocked
    found = s.find("combat(");
    if (found != string::npos)
    {
        size_t end = s.find(")",found);
        string combatTrigger = s.substr(found + 7, end - found - 7);
        //find combat traits, only trigger types, the general restrictions are found earlier.
        bool attackingTrigger = false;
        bool attackedAloneTrigger = false;
        bool notBlockedTrigger = false;
        bool attackBlockedTrigger = false;
        bool blockingTrigger = false;
        vector <string> combatTriggerVector = split(combatTrigger, ',');
        for (unsigned int i = 0 ; i < combatTriggerVector.size() ; i++)
        { 
            if(combatTriggerVector[i] == "attacking")
                attackingTrigger = true;
            if(combatTriggerVector[i] == "attackedalone")
                attackedAloneTrigger = true;
            if(combatTriggerVector[i] == "notblocked")
                notBlockedTrigger = true;
            if(combatTriggerVector[i] == "blocked")
                attackBlockedTrigger = true;
            if(combatTriggerVector[i] == "blocking")
                blockingTrigger = true;
        }  
        //build triggers TCs
        TargetChooser *tc = parseSimpleTC(s, "source", card);
        if(!tc)//a source( is required, from( is optional.
            return NULL;

        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        
        return NEW TrCombatTrigger(observer, id, card, tc, fromTc, once, limitOnceATurn, sourceUntapped, opponentPoisoned,
            attackingTrigger, attackedAloneTrigger, notBlockedTrigger, attackBlockedTrigger, blockingTrigger);
    }

    //poisoned player - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "poisonedof", card)){
        int plus = 0;
        bool duplicate = false;
        bool half = false;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        return NEW TrplayerPoisoned(observer, id, card, tc, once, true, false, plus, duplicate, half);
    }

    //poisoned player - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "poisonedfoeof", card)){
        int plus = 0;
        bool duplicate = false;
        bool half = false;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        return NEW TrplayerPoisoned(observer, id, card, tc, once, false, true, plus, duplicate, half);
    }

    //energized player - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "energizedof", card)){
        int plus = 0;
        bool duplicate = false;
        bool half = false;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        return NEW TrplayerEnergized(observer, id, card, tc, once, true, false, plus, duplicate, half);
    }

    //energized player - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "energizedfoeof", card)){
        int plus = 0;
        bool duplicate = false;
        bool half = false;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        return NEW TrplayerEnergized(observer, id, card, tc, once, false, true, plus, duplicate, half);
    }

    //experienced player - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "experiencedof", card)){
        int plus = 0;
        bool duplicate = false;
        bool half = false;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        return NEW TrplayerExperienced(observer, id, card, tc, once, true, false, plus, duplicate, half);
    }

    //experienced player - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "experiencedfoeof", card)){
        int plus = 0;
        bool duplicate = false;
        bool half = false;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        return NEW TrplayerExperienced(observer, id, card, tc, once, false, true, plus, duplicate, half);
    }

    //proliferated - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "proliferateof", card)){
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify an exception in order to avoid proliferation loop (eg. Tekuthal, Inquiry Dominus)
        if(exception)
            return NEW TrplayerProliferated(observer, id, card, tc, once, true, false, exception);
        else
            return NEW TrplayerProliferated(observer, id, card, tc, once, true, false);
    }

    //proliferated - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "proliferatefoeof", card)){
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify an exception in order to avoid proliferation loop (eg. Tekuthal, Inquiry Dominus)
        if(exception)
            return NEW TrplayerProliferated(observer, id, card, tc, once, false, true, exception);
        else
            return NEW TrplayerProliferated(observer, id, card, tc, once, false, true);
    }

    //ring temptations - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "ringtemptedof", card))
        return NEW TrplayerTempted(observer, id, card, tc, once, true, false);

    //ring temptations - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "ringtemptedfoeof", card))
        return NEW TrplayerTempted(observer, id, card, tc, once, false, true);

    //becomes monarch - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "becomesmonarchof", card))
        return NEW TrplayerMonarch(observer, id, card, tc, once, true, false);

    //becomes monarch - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "becomesmonarchfoeof", card))
        return NEW TrplayerMonarch(observer, id, card, tc, once, false, true);

    //takes the initiative - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "takeninitiativeof", card))
        return NEW TrplayerInitiative(observer, id, card, tc, once, true, false);

    //takes the initiative - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "takeninitiativefoeof", card))
        return NEW TrplayerInitiative(observer, id, card, tc, once, false, true);

    //shuffled library - controller of card
    if (TargetChooser * tc = parseSimpleTC(s, "shuffledof", card))
        return NEW TrplayerShuffled(observer, id, card, tc, once, true, false);

    //shuffled library - opponent of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "shuffledfoeof", card))
        return NEW TrplayerShuffled(observer, id, card, tc, once, false, true);

    //drawn player - controller of card - dynamic version drawof(player) -> returns current controller even with exchange of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "drawof", card))
        return NEW TrcardDrawn(observer, id, card, tc, once, true, false, limitOnceATurn);

    //drawn player - opponent of card controller - dynamic version drawfoeof(player) -> returns current opponent even with exchange of card controller
    if (TargetChooser * tc = parseSimpleTC(s, "drawfoeof", card))
        return NEW TrcardDrawn(observer, id, card, tc, once, false, true, limitOnceATurn);

    //Card card is drawn - static version - drawn(player) - any player; drawn(controller) - owner forever; drawn(opponent) - opponent forever
    if (TargetChooser * tc = parseSimpleTC(s, "drawn", card))
        return NEW TrcardDrawn(observer, id, card, tc,once);

    //Card is mutated
    if (TargetChooser * tc = parseSimpleTC(s, "mutated", card))
        return NEW TrCardMutated(observer, id, card, tc, once, limitOnceATurn);

    //boast has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "boasted", card))
        return NEW TrCardBoasted(observer, id, card, tc, once, limitOnceATurn);

    //a battle card has been defeated
    if (TargetChooser * tc = parseSimpleTC(s, "defeated", card))
        return NEW TrCardDefeated(observer, id, card, tc, once, limitOnceATurn);

    //Surveil has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "surveiled", card))
        return NEW TrCardSurveiled(observer, id, card, tc, once, limitOnceATurn);

    //Foretell has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "foretold", card))
        return NEW TrCardForetold(observer, id, card, tc, once, limitOnceATurn);

    //Train has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "trained", card))
        return NEW TrCardTrained(observer, id, card, tc, once, limitOnceATurn);

    //Scry has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "scryed", card))
        return NEW TrCardScryed(observer, id, card, tc, once, limitOnceATurn);

    //Ninjutsu has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "ninjutsued", card))
        return NEW TrCardNinja(observer, id, card, tc, once, limitOnceATurn);

    //Explores has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "explored", card))
        return NEW TrCardExplored(observer, id, card, tc, once, limitOnceATurn);

    //A Ring bearer has been chosen
    if (TargetChooser * tc = parseSimpleTC(s, "bearerchosen", card))
        return NEW TrCardBearerChosen(observer, id, card, tc, once, limitOnceATurn, false);

    //A different Ring bearer has been chosen
    if (TargetChooser * tc = parseSimpleTC(s, "bearernewchosen", card))
        return NEW TrCardBearerChosen(observer, id, card, tc, once, limitOnceATurn, true);

    //Dungeon has been completer from a card
    if (TargetChooser * tc = parseSimpleTC(s, "dungeoncompleted", card)){
        int totaldng = 0;
        vector<string>res = parseBetween(s, "total(",")");
        if(res.size()){
            totaldng = atoi(res[1].c_str());
        }
        string playerName = "";
        vector<string>from = parseBetween(s, "from(",")");
        if(from.size() && from[1] == "opponent"){
            playerName = card->controller()->opponent()->getDisplayName();
        } else if(from.size() && from[1] == "controller"){
            playerName = card->controller()->getDisplayName();
        } 
        return NEW TrCardDungeonCompleted(observer, id, card, tc, once, limitOnceATurn, totaldng, playerName);
    }

    //Roll die has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "dierolled", card)){
        int rollresult = 0;
        vector<string>res = parseBetween(s, "result(",")");
        if(res.size()){
            if(res[1] == "max"){
                rollresult = -1;
            } else {
                rollresult = atoi(res[1].c_str());
            }
        }
        string playerName = "";
        vector<string>from = parseBetween(s, "from(",")");
        if(from.size() && from[1] == "opponent"){
            playerName = card->controller()->opponent()->getDisplayName();
        } else if(from.size() && from[1] == "controller"){
            playerName = card->controller()->getDisplayName();
        } 
        return NEW TrCardRolledDie(observer, id, card, tc, once, limitOnceATurn, rollresult, playerName);
    }

    //Fip coin has been performed from a card
    if (TargetChooser * tc = parseSimpleTC(s, "coinflipped", card)){
        int flipresult = -1;
        vector<string>res = parseBetween(s, "result(",")");
        if(res.size() && res[1] == "head"){
            flipresult = 0;
        } else if(res.size() && (res[1] == "tails" || res[1] == "tail")){
            flipresult = 1;
        } else if(res.size() && res[1] == "won"){
            flipresult = 2;
        } else if(res.size() && res[1] == "lost"){
            flipresult = 3;
        }
        string playerName = "";
        vector<string>from = parseBetween(s, "from(",")");
        if(from.size() && from[1] == "opponent"){
            playerName = card->controller()->opponent()->getDisplayName();
        } else if(from.size() && from[1] == "controller"){
            playerName = card->controller()->getDisplayName();
        }
        return NEW TrCardFlippedCoin(observer, id, card, tc, once, limitOnceATurn, flipresult, playerName);
    }

    //Token has been created
    if (TargetChooser * tc = parseSimpleTC(s, "tokencreated", card))
        return NEW TrTokenCreated(observer, id, card, tc, once, limitOnceATurn);

    //Card is sacrificed
    if (TargetChooser * tc = parseSimpleTC(s, "sacrificed", card))
        return NEW TrCardSacrificed(observer, id, card, tc, once, limitOnceATurn);

    //Card is exploited
    if (TargetChooser * tc = parseSimpleTC(s, "exploited", card))
        return NEW TrCardExploited(observer, id, card, tc, once, limitOnceATurn);

    //Card is Discarded
    if (TargetChooser * tc = parseSimpleTC(s, "discarded", card))
        return NEW TrCardDiscarded(observer, id, card, tc, once, limitOnceATurn);

    //Card is cycled
    if (TargetChooser * tc = parseSimpleTC(s, "cycled", card))
        return NEW TrCardDiscarded(observer, id, card, tc, once, limitOnceATurn, true);

    //Card Damaging non combat current controller
    if (TargetChooser * tc = parseSimpleTC(s, "noncombatdamageof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 2, false, false, once, true, false);
    }

    //Card Damaging non combat current opponent
    if (TargetChooser * tc = parseSimpleTC(s, "noncombatdamagefoeof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 2, false, false, once, false, true);
    }

    //Card Damaging non combat static
    if (TargetChooser * tc = parseSimpleTC(s, "noncombatdamaged", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 2, once);
    }

    //Card Damaging combat current controller
    if (TargetChooser * tc = parseSimpleTC(s, "combatdamageof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 1, sourceUntapped, limitOnceATurn, once, true, false);
    }

    //Card Damaging combat current opponent
    if (TargetChooser * tc = parseSimpleTC(s, "combatdamagefoeof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 1, sourceUntapped, limitOnceATurn, once, false, true);
    }

    //Card Damaging combat static
    if (TargetChooser * tc = parseSimpleTC(s, "combatdamaged", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 1, sourceUntapped, limitOnceATurn, once);
    }

    //Card Damaging current controller
    if (TargetChooser * tc = parseSimpleTC(s, "damageof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 0, sourceUntapped, limitOnceATurn, once, true, false);
    }

    //Card Damaging current opponent
    if (TargetChooser * tc = parseSimpleTC(s, "damagefoeof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 0, sourceUntapped, limitOnceATurn, once, false, true);
    }

    //Card Damaging static
    if (TargetChooser * tc = parseSimpleTC(s, "damaged", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrDamaged(observer, id, card, tc, fromTc, 0, sourceUntapped, limitOnceATurn, once);
    }

    //Lifed current controller
    if (TargetChooser * tc = parseSimpleTC(s, "lifeof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a life gain/loss card exception in order to avoid life gain loop.
        if(exception)
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 0, sourceUntapped, once, true, false, limitOnceATurn, exception);
        else
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 0, sourceUntapped, once, true, false, limitOnceATurn);
    }

    //Lifed current opponent
    if (TargetChooser * tc = parseSimpleTC(s, "lifefoeof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a life gain/loss card exception in order to avoid life gain loop.
        if(exception)
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 0, sourceUntapped,once, false, true, limitOnceATurn, exception); 
        else
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 0, sourceUntapped,once, false, true, limitOnceATurn);
    }

    //Lifed static
    if (TargetChooser * tc = parseSimpleTC(s, "lifed", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a life gain/loss card exception in order to avoid life gain loop.
        if(exception)
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 0, sourceUntapped, once, false, false, limitOnceATurn, exception);
        else
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 0, sourceUntapped, once, false, false, limitOnceATurn);
    }

    //Life Loss current player
    if (TargetChooser * tc = parseSimpleTC(s, "lifelostof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a life gain/loss card exception in order to avoid life gain loop.
        if(exception)
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 1, sourceUntapped, once, true, false, limitOnceATurn, exception);
        else
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 1, sourceUntapped, once, true, false, limitOnceATurn);
    }

    //Life Loss current opponent
    if (TargetChooser * tc = parseSimpleTC(s, "lifelostfoeof", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a life gain/loss card exception in order to avoid life gain loop.
        if(exception)
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 1, sourceUntapped, once, false, true, limitOnceATurn,exception);
        else
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 1, sourceUntapped, once, false, true, limitOnceATurn);
    }

    //Life Loss static
    if (TargetChooser * tc = parseSimpleTC(s, "lifeloss", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a life gain/loss card exception in order to avoid life gain loop.
        if(exception)
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 1, sourceUntapped, once, false, false, limitOnceATurn, exception);
        else
            return NEW TrLifeGained(observer, id, card, tc, fromTc, 1, sourceUntapped, once, false, false, limitOnceATurn);
    }

    //Card Damaged and killed by a creature this turn
    if (TargetChooser * tc = parseSimpleTC(s, "vampired", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrVampired(observer, id, card, tc, fromTc, once, limitOnceATurn);
    }

    //when card becomes the target of a spell or ability
    if (TargetChooser * tc = parseSimpleTC(s, "targeted", card))
    {
        TargetChooser *fromTc = parseSimpleTC(s, "from", card);
        return NEW TrTargeted(observer, id, card, tc, fromTc, 0, once, limitOnceATurn);
    }

    if (s.find("totalcounteradded(") != string::npos)
    {
        vector<string>splitCounter = parseBetween(s,"totalcounteradded(",")");
        Counter * counter = NULL;
        bool nocost = false;
        bool duplicate = false;
        bool half = false;
        int plus = 0;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        if(s.find("(any)") == string::npos)
            counter = parseCounter(splitCounter[1],card,NULL);
        if(s.find("nocost") != string::npos)
            nocost = true;
        TargetChooser * tc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a counter add/remove exception in order to avoid counter loop.
        if(exception)
            return NEW TrTotalCounter(observer, id, card, counter, tc, 1, once, duplicate, half, plus, nocost, limitOnceATurn, exception);
        else
            return NEW TrTotalCounter(observer, id, card, counter, tc, 1, once, duplicate, half, plus, nocost, limitOnceATurn);
    }

    if (s.find("totalcounterremoved(") != string::npos)
    {
        vector<string>splitCounter = parseBetween(s,"totalcounterremoved(",")");
        Counter * counter = NULL;
        bool nocost = false;
        bool duplicate = false;
        bool half = false;
        int plus = 0;
        if(s.find("plus(1)") != string::npos)
            plus = 1;
        else if(s.find("plus(2)") != string::npos)
            plus = 2;
        else if(s.find("plus(3)") != string::npos)
            plus = 3;
        else if(s.find("plus(4)") != string::npos)
            plus = 4;
        else if(s.find("plus(5)") != string::npos)
            plus = 5;
        else if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        else if(s.find("half(all)") != string::npos)
            half = true;
        if(s.find("(any)") == string::npos)
            counter = parseCounter(splitCounter[1],card,NULL);
        if(s.find("nocost") != string::npos)
            nocost = true;
        TargetChooser * tc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a counter add/remove exception in order to avoid counter loop.
        if(exception)
            return NEW TrTotalCounter(observer, id, card, counter, tc, 0, once, duplicate, half, plus, nocost, limitOnceATurn, exception);
        else
            return NEW TrTotalCounter(observer, id, card, counter, tc, 0, once, duplicate, half, plus, nocost, limitOnceATurn);
    }

    if (s.find("counteradded(") != string::npos)
    {
        vector<string>splitCounter = parseBetween(s,"counteradded(",")");
        Counter * counter = NULL;
        bool duplicate = false;
        if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        if(s.find("(any)") == string::npos)
            counter = parseCounter(splitCounter[1],card,NULL);
        TargetChooser * tc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a counter add/remove exception in order to avoid counter loop.
        if(exception)
            return NEW TrCounter(observer, id, card, counter, tc, 1, once, duplicate, limitOnceATurn, exception);
        else
            return NEW TrCounter(observer, id, card, counter, tc, 1, once, duplicate, limitOnceATurn);
    }

    if (s.find("counterremoved(") != string::npos)
    {
        vector<string>splitCounter = parseBetween(s,"counterremoved(",")");
        Counter * counter = NULL;
        bool duplicate = false;
        if(s.find("duplicate(all)") != string::npos)
            duplicate = true;
        if(s.find("(any)") == string::npos)
            counter = parseCounter(splitCounter[1],card,NULL);
        TargetChooser * tc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a counter add/remove exception in order to avoid counter loop.
        if(exception)
            return NEW TrCounter(observer, id, card, counter, tc, 0, once, duplicate, limitOnceATurn, exception);
        else
            return NEW TrCounter(observer, id, card, counter, tc, 0, once, duplicate, limitOnceATurn);
    }

    if (s.find("countermod(") != string::npos)
    {
        vector<string>splitCounter = parseBetween(s,"countermod(",")");
        Counter * counter = NULL;
        if(s.find("(any)") == string::npos)
            counter = parseCounter(splitCounter[1],card,NULL);
        TargetChooser * tc = parseSimpleTC(s, "from", card);
        TargetChooser *exception = parseSimpleTC(s, "except", card); // Added a new keyword except to specify a counter add/remove exception in order to avoid counter loop.
        if(exception)
            return NEW TrCounter(observer, id, card, counter, tc, 2, once, false, limitOnceATurn, exception);
        else
            return NEW TrCounter(observer, id, card, counter, tc, 2, once, false, limitOnceATurn);
    }

    int who = 0;
    if (s.find(" my") != string::npos)
        who = 1;
    if (s.find(" opponent") != string::npos)
        who = -1;
    if (s.find(" targetcontroller") != string::npos)
        who = -2;
    if (s.find(" targetedplayer") != string::npos)
        who = -3;

    //Next Time...
    found = s.find("next");
    if (found != string::npos)
    {
        for (int i = 0; i < NB_MTG_PHASES; i++)
        {
            found = s.find(Constants::MTGPhaseCodeNames[i]);
            if (found != string::npos)
            {
                return NEW TriggerNextPhase(observer, id, card, target, i, who, sourceUntapped, once);
            }
        }
    }

    //Each Time...
    found = s.find("each");
    if (found != string::npos)
    {
        for (int i = 0; i < NB_MTG_PHASES; i++)
        {
            found = s.find(Constants::MTGPhaseCodeNames[i]);
            if (found != string::npos)
            {
                return NEW TriggerAtPhase(observer, id, card, target, i, who, sourceUntapped, sourceTap, lifelost, lifeamount, once);
            }
        }
    }

    //rebound trigger controller upkeep...
    found = s.find("rebounded");
    if (found != string::npos)
    {
        return NEW TriggerRebound(observer, id, card, target, 2, 1, sourceUntapped, once);
    }

    return NULL;
}

// When abilities encapsulate each other, gets the deepest one (it is the one likely to have the most relevant information)
MTGAbility * AbilityFactory::getCoreAbility(MTGAbility * a)
{
    if(AForeach * fea = dynamic_cast<AForeach*>(a))
        return getCoreAbility(fea->ability);
        
    if( AAsLongAs * aea = dynamic_cast<AAsLongAs*>(a))
        return getCoreAbility(aea->ability);
        
    if (GenericTargetAbility * gta = dynamic_cast<GenericTargetAbility*> (a))
        return getCoreAbility(gta->ability);

    if (GenericActivatedAbility * gaa = dynamic_cast<GenericActivatedAbility*> (a))
        return getCoreAbility(gaa->ability);

    if (MultiAbility * abi = dynamic_cast<MultiAbility*>(a))
        return getCoreAbility(abi->abilities[0]);

    if (NestedAbility * na = dynamic_cast<NestedAbility*> (a))
    {
        if(na->ability)
            //only atempt to return a nestedability if it contains a valid ability. example where this causes a bug otherwise. AEquip is considered nested, but contains no ability.
            return getCoreAbility(na->ability);
    }
    
    if (MenuAbility * ma = dynamic_cast<MenuAbility*>(a))
        return getCoreAbility(ma->abilities[0]);

    return a;
}

//Parses a string and returns the corresponding MTGAbility object
//Returns NULL if parsing failed
//Beware, Spell CAN be null when the function is called by the AI trying to analyze the effects of a given card
MTGAbility * AbilityFactory::parseMagicLine(string s, int id, Spell * spell, MTGCardInstance *card, bool activated, bool forceUEOT,
                                            MTGGameZone * dest)
{
    size_t found;
    bool asAlternate = false;
    trim(s);
    //TODO This block redundant with calling function
    if (!card && spell)
        card = spell->source;
    if (!card)
        return NULL;
    MTGCardInstance * target = card->target;
    if (!target)
        target = card;
    //pay and castcard?
    if(s.find("castcard(restricted") != string::npos && (s.find("pay(") != string::npos || s.find("pay[[") != string::npos))
        asAlternate = true;
    //MTG Specific rules
    //adds the bonus credit system
    found = s.find("bonusrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGEventBonus(observer, -1));
        return NULL;
    }
    //putinplay/cast rule.. this is a parent rule and is required for all cost related rules.
    found = s.find("putinplayrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGPutInPlayRule(observer, -1));
        return NULL;
    }
    //rule for kicker handling
    found = s.find("kickerrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGKickerRule(observer, -1));
        return NULL;
    }
    //alternative cost types rule, this is a parent rule and is required for all cost related rules.
    found = s.find("alternativecostrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGAlternativeCostRule(observer, -1));
        return NULL;
    }
    //alternative cost type buyback
    found = s.find("buybackrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGBuyBackRule(observer, -1));
        return NULL;
    }
    //alternative cost type flashback
    found = s.find("flashbackrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGFlashBackRule(observer, -1));
        observer->addObserver(NEW MTGTempFlashBackRule(observer, -1));
        return NULL;
    }
    //alternative cost type bestow
    found = s.find("bestowrule");
    if (found != string::npos)
    {
        observer->addObserver(NEW MTGBestowRule(observer, -1));
        return NULL;
    }
    //alternative cost type retrace
    found = s.find("retracerule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGRetraceRule(observer, -1));
        return NULL;
    }
    //alternative cost type suspend
    found = s.find("suspendrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGSuspendRule(observer, -1));
        return NULL;
    }
    //alternative cost type morph
    found = s.find("morphrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGMorphCostRule(observer, -1));
        return NULL;
    }
    found = s.find("payzerorule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGPayZeroRule(observer, -1));
        return NULL;
    }
    found = s.find("overloadrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGOverloadRule(observer, -1));
        return NULL;
    }
    //this rule handles attacking ability during attacker phase
    found = s.find("attackrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGAttackRule(observer, -1));
        return NULL;
    }
    //this rule handles attacking cost ability during attacker phase
    found = s.find("attackcostrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGAttackCostRule(observer, -1));
        return NULL;
    }
    //this rule handles blocking ability during blocker phase
    found = s.find("blockrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGBlockRule(observer, -1));
        return NULL;
    }
    //this rule handles blocking cost ability during blocker phase
    found = s.find("blockcostrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGBlockCostRule(observer, -1));
        return NULL;
    }
    //this rule handles cards that have soulbond
    found = s.find("soulbondrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGSoulbondRule(observer, -1));
        return NULL;
    }
    //this rule handles cards that have dredge
    found = s.find("dredgerule");
    if(found != string::npos)
    {
        observer->replacementEffects->add(NEW MTGDredgeRule(observer, -1));
        return NULL;
    }
    //this rule handles combat related triggers. note, combat related triggered abilities will not work without it.
    found = s.find("combattriggerrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGCombatTriggersRule(observer, -1));
        return NULL;
    }
    //this handles the legend rule
    found = s.find("legendrule");
    if(found != string::npos)
    {
        //observer->addObserver(NEW MTGLegendRule(observer, -1));
        observer->addObserver(NEW MTGNewLegend(observer, -1));
        return NULL;
    }
    //this handles the planeswalker named legend rule which is dramatically different from above.
    found = s.find("planeswalkerrule");
    if(found != string::npos)
    {
        //observer->addObserver(NEW MTGPlaneWalkerRule(observer, -1));
        observer->addObserver(NEW MTGNewPlaneswalker(observer, -1));
        return NULL;
    }
    found = s.find("planeswalkerdamage");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGPlaneswalkerDamage(observer, -1));
        return NULL;
    }
    found = s.find("planeswalkerattack");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGPlaneswalkerAttackRule(observer, -1));
        return NULL;
    }
    
        //this handles the clean up of tokens !!MUST BE ADDED BEFORE PERSIST RULE!!
    found = s.find("tokencleanuprule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGTokensCleanup(observer, -1));
        return NULL;
    }
        //this handles the returning of cards with persist to the battlefield.
    found = s.find("persistrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGPersistRule(observer, -1));
        return NULL;
    }
    //this handles the vectors of cards which attacked and were attacked and later died during that turn.
    found = s.find("vampirerule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGVampireRule(observer, -1));
        return NULL;
    }
    //this handles the removel of cards which were unearthed.
    found = s.find("unearthrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGUnearthRule(observer, -1));
        return NULL;
    }
    //this handles lifelink ability rules.
    found = s.find("lifelinkrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGLifelinkRule(observer, -1));
        return NULL;
    }
    //this handles death touch ability rule.
    found = s.find("deathtouchrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW MTGDeathtouchRule(observer, -1));
        return NULL;
    }

    if (StartsWith(s, "chooseacolor ") || StartsWith(s, "chooseatype ") || StartsWith(s, "chooseaname"))
    {
        if (storedAbilityString.size() && StartsWith(s, "chooseaname"))
        {
            bool chooseoppo = false;
            vector<string> splitChoose = parseBetween(s, "chooseaname ", " chooseend", false);
            if(!splitChoose.size()){
                splitChoose = parseBetween(s, "chooseanameopp ", " chooseend", false);
                chooseoppo = true;
            }
            if (splitChoose.size())
            {
                if(!chooseoppo)
                    s = "chooseaname ";
                else
                    s = "chooseanameopp ";

                s.append(storedAbilityString);
                s.append(" ");
                if(splitChoose[2].empty())
                    s.append(splitChoose[1]);
                else
                    s.append(splitChoose[2]);
            }
        }
        MTGAbility * choose = parseChooseActionAbility(s, card, spell, target, 0, id);
        choose = NEW GenericActivatedAbility(observer, "", "", id, card, choose, NULL);
        MayAbility * mainAbility = NEW MayAbility(observer, id, choose, card, true);
        return mainAbility;
    }

    //need to remove the section inside the transforms ability from the string before parsing
    //TODO: store string values of "&&" so we can remove the classes added just to add support
    //the current parser finds other abilities inside what should be nested abilities, and converts them into
    //actual abilities, this is a limitation.
    string unchangedS = "";
    unchangedS.append(s);

    //Reveal:x remove the core so we dont build them prematurely
    vector<string>transPayfound = parseBetween(s, "newability[pay(", " ");
    vector<string>transfound = parseBetween(s,"newability[reveal:"," ");//if we are using reveal inside a newability, let transforms remove the string instead.    
    vector<string>abilfound = parseBetween(s, "ability$!name(reveal) reveal:", " ");
    if(!abilfound.size())
        abilfound = parseBetween(s, "ability$!reveal:", " ");//see above. this allows us to nest reveals inside these 2 other master classes. while also allowing us to nest them inside reveals.
    
    found = s.find("pay(");
    if (found != string::npos && storedPayString.empty() && !transPayfound.size())
    {
        size_t pos1 = s.find("transforms(("); // Try to handle pay ability inside ability$! or transforms keywords.
        size_t pos2 = s.find("ability$!");
        if(pos2 == string::npos)
            pos2 = s.find("winability"); // Try to handle pay ability inside winability or loseability keywords.
        if(pos2 == string::npos)
            pos2 = s.find("loseability");
        vector<string> splitMayPaystr = parseBetween(s, "pay(", ")", true);
        if((pos1 == string::npos && pos2 == string::npos) || (pos2 != string::npos && pos1 != string::npos && found < pos1 && found < pos2) || 
            (pos2 == string::npos && pos1 != string::npos && found < pos1) || (pos1 == string::npos && pos2 != string::npos && found < pos2)){
            if (splitMayPaystr.size()){
                storedPayString.append(splitMayPaystr[2]);
                s = splitMayPaystr[0];
                s.append("pay(");
                s.append(splitMayPaystr[1]);
                s.append(")");
            } 
        } else 
              storedPayString.clear();
    }

    bool chooseoppo = false;
    vector<string> splitChoose = parseBetween(s, "chooseaname ", " chooseend", false);
    if(!splitChoose.size()){
        splitChoose = parseBetween(s, "chooseanameopp ", " chooseend", false);
        chooseoppo = true;
    }
    if (splitChoose.size() && storedAbilityString.empty())
    {
        storedAbilityString = splitChoose[1];
        size_t pos1 = s.find("transforms(("); // Try to handle chooseaname ability inside ability$! or transforms keywords.
        size_t pos2 = s.find("ability$!");
        if(pos2 == string::npos)
            pos2 = s.find("winability"); // Try to handle chooseaname ability inside winability or loseability keywords.
        if(pos2 == string::npos)
            pos2 = s.find("loseability");
        size_t pos3 = s.find(splitChoose[1]);
        if((pos1 == string::npos && pos2 == string::npos) || (pos2 != string::npos && pos1 != string::npos && pos3 <= pos1 && pos3 <= pos2) || 
            (pos2 == string::npos && pos1 != string::npos && pos3 <= pos1) || (pos1 == string::npos && pos2 != string::npos && pos3 <= pos2)){
            s = splitChoose[0];
            if(!chooseoppo)
                s.append("chooseaname ");
            else
                s.append("chooseanameopp ");
            s.append(splitChoose[2]);
        } else 
            storedAbilityString.clear();
    }

    vector<string> splitGrant = parseBetween(s, "grant ", " grantend", false);
    if (splitGrant.size() && storedAbilityString.empty())
    {
        storedAbilityString = splitGrant[1];
        size_t pos1 = s.find("transforms(("); // Try to handle grant ability inside ability$! or transforms keywords.
        size_t pos2 = s.find("ability$!");
        if(pos2 == string::npos)
            pos2 = s.find("winability"); // Try to handle grant ability inside winability or loseability keywords.
        if(pos2 == string::npos)
            pos2 = s.find("loseability");
        size_t pos3 = s.find(splitGrant[1]);
        if((pos1 == string::npos && pos2 == string::npos) || (pos2 != string::npos && pos1 != string::npos && pos3 <= pos1 && pos3 <= pos2) || 
            (pos2 == string::npos && pos1 != string::npos && pos3 <= pos1) || (pos1 == string::npos && pos2 != string::npos && pos3 <= pos2)){
            s = splitGrant[0];
            s.append("grant ");
            s.append(splitGrant[2]);
        } else 
            storedAbilityString.clear();
    }

    vector<string> splitRevealx = parseBetween(s, "reveal:", " revealend", false);
    if (!abilfound.size() && !transfound.size() && splitRevealx.size() && storedAbilityString.empty())
    {
        storedAbilityString = splitRevealx[1];
        size_t pos1 = s.find("transforms(("); // Try to handle reveal ability inside ability$! or transforms keywords.
        size_t pos2 = s.find("ability$!");
        if(pos2 == string::npos)
            pos2 = s.find("winability"); // Try to handle reveal ability inside winability or loseability keywords.
        if(pos2 == string::npos)
            pos2 = s.find("loseability");
        size_t pos3 = s.find(splitRevealx[1]);
        if((pos1 == string::npos && pos2 == string::npos) || (pos2 != string::npos && pos1 != string::npos && pos3 <= pos1 && pos3 <= pos2) || 
            (pos2 == string::npos && pos1 != string::npos && pos3 <= pos1) || (pos1 == string::npos && pos2 != string::npos && pos3 <= pos2)){
            s = splitRevealx[0];
            s.append("reveal: ");
            s.append(splitRevealx[2]);
        } else 
            storedAbilityString.clear();
    }

    vector<string> splitScryx = parseBetween(s, "scry:", " scryend", false);
    if (splitScryx.size() && storedAbilityString.empty())
    {
        storedAbilityString = splitScryx[1];
        size_t pos1 = s.find("transforms(("); // Try to handle scry ability inside ability$! or transforms keywords.
        size_t pos2 = s.find("ability$!");
        if(pos2 == string::npos)
            pos2 = s.find("winability"); // Try to handle scry ability inside winability or loseability keywords.
        if(pos2 == string::npos)
            pos2 = s.find("loseability");
        size_t pos3 = s.find(splitScryx[1]);
        if((pos1 == string::npos && pos2 == string::npos) || (pos2 != string::npos && pos1 != string::npos && pos3 <= pos1 && pos3 <= pos2) || 
            (pos2 == string::npos && pos1 != string::npos && pos3 <= pos1) || (pos1 == string::npos && pos2 != string::npos && pos3 <= pos2)){
            s = splitScryx[0];
            s.append("scry: ");
            s.append(splitScryx[2]);
        } else 
            storedAbilityString.clear();
    }

    found = s.find("transforms((");
    if (found != string::npos && storedString.empty())
    {
        size_t pos1 = s.find("ability$!"); // Try to handle transforms ability inside ability$! keyword.
        if(pos1 == string::npos)
            pos1 = s.find("winability"); // Try to handle transforms ability inside winability or loseability keywords.
        if(pos1 == string::npos)
            pos1 = s.find("loseability");
        if(pos1 == string::npos || found < pos1){
            size_t real_end = s.find("))", found);
            size_t stypesStartIndex = found + 12;
            for(unsigned int i = stypesStartIndex; i < s.size(); i++){
                size_t pos2 = s.find("transforms((", i); // Try to handle transforms ability inside transforms keyword.
                if(pos2 != string::npos && pos2 < real_end){
                    i = pos2 + 11;
                    real_end = s.find("))", real_end + 2);
                } else
                    break;
            }
            storedString.append(s.substr(stypesStartIndex, real_end - stypesStartIndex).c_str());
            s.erase(stypesStartIndex, real_end - stypesStartIndex);
        }
    }

    found = s.find("ability$!");
    if (found != string::npos && storedAbilityString.empty())
    {
        size_t real_end = s.find("!$", found);
        size_t sIndex = found + 9;
        storedAbilityString.append(s.substr(sIndex, real_end - sIndex).c_str());
        s.erase(sIndex, real_end - sIndex);
    }
    else
    {
        found = unchangedS.find("ability$!");//did find it in a changed s, try unchanged.
        if (found != string::npos && storedAbilityString.empty())
        {
            size_t real_end = unchangedS.find("!$", found);
            size_t sIndex = found + 9;
            storedAbilityString.append(unchangedS.substr(sIndex, real_end - sIndex).c_str());
            unchangedS.erase(sIndex, real_end - sIndex);
        }
    }

    found = s.find("pay[[");
    if (found != string::npos && storedPayString.empty())
    {
        vector<string> splitMayPaystr = parseBetween(s, "pay[[", " ", true);
        if(splitMayPaystr.size())
        {
            storedPayString.append(splitMayPaystr[2]);
            s = splitMayPaystr[0];
            s.append("pay[[");
            s.append(splitMayPaystr[1]);
            s.append("]]");
        }
    }

    found = s.find("and!(");
    if (found != string::npos && found + 6 != ')' && storedAndAbility.empty())
    {
        vector<string> splitAnd = parseBetween(s, "and!(", ")!",false);
        if(splitAnd.size())
        {
            storedAndAbility.append(splitAnd[1]);
            size_t real_end = s.find(")!", found);
            size_t sIndex = found + 5;
            s.erase(sIndex, real_end - sIndex);
        }
    }

    vector<string> splitTrigger = parseBetween(s, "@", ":");
    if (splitTrigger.size())
    {
        TriggeredAbility * trigger = parseTrigger(splitTrigger[1], s, id, spell, card, target);
        if (splitTrigger[1].find("restriction{") != string::npos)//using other/cast restrictions for abilities.
        {
            vector<string> splitRest = parseBetween(s,"restriction{","}");
            if (splitRest.size())
                trigger->castRestriction = splitRest[1];
        }
        if (splitTrigger[1].find("restriction{{") != string::npos)
        {
            vector<string> splitRest = parseBetween(s,"restriction{{","}}");
            if (splitRest.size())
                trigger->castRestriction = splitRest[1];
        }
        //Dirty way to remove the trigger text (could get in the way)
        if (trigger)
        {
            MTGAbility * a = parseMagicLine(splitTrigger[2], id, spell, card, activated);
            if (!a)
            {
                delete trigger;
                return NULL;
            }
            return NEW GenericTriggeredAbility(observer, id, card, trigger, a, NULL, target);
        }
    }

    //This one is not a real ability, it displays a message on the screen. We use this for tutorials
    // Triggers need to be checked above this one, as events are usuallly what will trigger (...) these messages
    {
        vector<string> splitMsg = parseBetween(s, "tutorial(", ")");
        if (splitMsg.size())
        {
            string msg = splitMsg[1];
            return NEW ATutorialMessage(observer, card, msg);
        }

        splitMsg = parseBetween(s, "message(", ")");
        if (splitMsg.size())
        {
            string msg = splitMsg[1];
            return NEW ATutorialMessage(observer, card, msg, 0);
        }
    }

    int restrictions = parseRestriction(s);
    string castRestriction = "";
    if (s.find("restriction{") != string::npos)//using other/cast restrictions for abilities.
    {
        vector<string> splitRest = parseBetween(s,"restriction{","}");
        if (splitRest.size())
            castRestriction = splitRest[1];
    }
    if (s.find("restriction{{") != string::npos)
    {
        vector<string> splitRest = parseBetween(s,"restriction{{","}}");
        if (splitRest.size())
            castRestriction = splitRest[1];
    }
    string newName = "";
    vector<string> splitName = parseBetween(s, "name(", ")");
    if (splitName.size())
    {
        newName = splitName[1];
        s = splitName[0];
        s.append(splitName[2]);
        //we erase the name section from the string to avoid 
        //accidently building an mtg ability with the text meant for menuText.
    }

    TargetChooser * tc = NULL;
    string sWithoutTc = s;
    string tcString = "";
    //Target Abilities - We also handle the case "notatarget" here, for things such as copy effects
    bool isTarget = true;
    vector<string> splitTarget = parseBetween(s, "notatarget(", ")");
    if (splitTarget.size())
        isTarget = false;
    else
        splitTarget = parseBetween(s, "target(", ")");

    if (splitTarget.size())
    {
        TargetChooserFactory tcf(observer);
        tc = tcf.createTargetChooser(splitTarget[1], card);
        tcString = splitTarget[1];

        if (!isTarget)
        {
            tc->targetter->bypassTC = true;
            tc->targetter = NULL;
        }
        else
            if (tc->targetter)
                tc->targetter->bypassTC = false;
        sWithoutTc = splitTarget[0];
        sWithoutTc.append(splitTarget[2]);
    }

    size_t delimiter = sWithoutTc.find("}:");
    size_t firstNonSpace = sWithoutTc.find_first_not_of(" ");
    if (delimiter != string::npos && firstNonSpace != string::npos && sWithoutTc[firstNonSpace] == '{')
    {
        ManaCost * cost = ManaCost::parseManaCost(sWithoutTc.substr(0, delimiter + 1), NULL, card);
        int doTap = 0; //Tap in the cost ?
        if(cost && cost->extraCosts)
        {
            for(unsigned int i = 0; i < cost->extraCosts->costs.size();i++)
            {
                ExtraCost * tapper = dynamic_cast<TapCost*>(cost->extraCosts->costs[i]);
                if(tapper)
                    doTap = 1;

            }
        }
        if (doTap || cost)
        {
            string s1 = sWithoutTc.substr(delimiter + 2);
            //grabbing the sideffect string and amount before parsing abilities.
            //side effect ei:dragond whelp.
            MTGAbility * sideEffect = NULL;
            string usesBeforeSideEffect = "";
            size_t limiteffect_str = s1.find("limit^");
            if (limiteffect_str != string::npos)
            {
            size_t end = s1.rfind("^");
            string sideEffectStr = s1.substr(limiteffect_str + 6,end - limiteffect_str - 6);
            s1.erase(limiteffect_str,end - limiteffect_str);
            end = s1.find("^");
            usesBeforeSideEffect = s1.substr(end+1);
            s1.erase(end-1);
            sideEffect = parseMagicLine(sideEffectStr, id, spell, card, 1);
            }
            
            
            MTGAbility * a = parseMagicLine(s1, id, spell, card, 1);
            if (!a)
            {
                DebugTrace("ABILITYFACTORY Error parsing: " << sWithoutTc);
                return NULL;
            }
            string limit = "";
            size_t limit_str = sWithoutTc.find("limit:");
            if (limit_str != string::npos)
            {
                limit = sWithoutTc.substr(limit_str + 6);
            }
            ////A stupid Special case for ManaProducers, becuase Ai only understands manaabilities that are not nested.
            AManaProducer * amp = dynamic_cast<AManaProducer*> (a);
            if (amp)
            {
                amp->setCost(cost);
                if (cost)
                    cost->setExtraCostsAction(a, card);
                amp->oneShot = 0;
                amp->tap = doTap;
                amp->limit = limit;
                amp->sideEffect = sideEffect;
                amp->usesBeforeSideEffects = usesBeforeSideEffect;
                amp->restrictions = restrictions;
                amp->castRestriction = castRestriction;
                return amp;
            }
            
            AEquip *ae = dynamic_cast<AEquip*> (a);
            if (ae)
            {
                ae->setCost(cost);
                if (!tc)
                {
                    TargetChooserFactory tcf(observer);
                    tc = tcf.createTargetChooser("creature|mybattlefield", card);
                }
                ae->setActionTC(tc);
                return ae;
            }
            if (tc)
            {
                tc->belongsToAbility = sWithoutTc;
                return NEW GenericTargetAbility(observer, newName,castRestriction,id, card, tc, a, cost, limit,sideEffect,usesBeforeSideEffect, restrictions, dest,tcString);
            }
            return NEW GenericActivatedAbility(observer, newName,castRestriction,id, card, a, cost, limit,sideEffect,usesBeforeSideEffect,restrictions, dest);
        }
        SAFE_DELETE(cost);
    }

    // figure out alternative cost effects
    for (size_t i = 0; i < sizeof(kAlternateCostIds)/sizeof(kAlternateCostIds[0]); ++i)
    {
        const string& keyword = kAlternateCostKeywords[i];
        if (s.find(keyword) == 0)
        {
            if (!(spell && spell->FullfilledAlternateCost(kAlternateCostIds[i])))
            {
                DebugTrace("INFO parseMagicLine: Alternative Cost was not fulfilled for " << spell << s);
                SAFE_DELETE(tc);
                return NULL;
            }
            return parseMagicLine(s.substr(keyword.length()), id, spell, card);
        }
    }
    //if/ifnot COND then DO EFFECT.
    const string ifKeywords[] = {"if ", "ifnot "};
    int checkIf[] = { 1, 2 };
    for (size_t i =0; i < sizeof(checkIf)/sizeof(checkIf[0]); ++i)
    {
        if (sWithoutTc.find(ifKeywords[i]) == 0)
        {
            string cond = sWithoutTc.substr(ifKeywords[i].length(),ifKeywords[i].length() + sWithoutTc.find(" then ")-6);
            size_t foundElse = s.find(" else ");
            MTGAbility * a2 = NULL;
            if(foundElse != string::npos)
            {
                string s2 = s.substr(foundElse+6);
                if(s2.size())
                {
                    s.erase(s.find(" else ")+1);
                    a2 = parseMagicLine(s2, id, spell, card);
                }
            }
            string s1 = s;
            MTGAbility * a1 = NULL;
            if (s1.find(" then ") != string::npos)
            a1 = parseMagicLine(s1.substr(s1.find(" then ") + 6), id, spell, card);
            if(!a1) return NULL;
            MTGAbility * a = NEW IfThenAbility(observer, id, a1,a2, card,(Targetable*)target,checkIf[i],cond);
            a->canBeInterrupted = false;
            a->oneShot = true;
            if(tc)
                SAFE_DELETE(tc);
            return a;
        }
    }
    //may pay ability
    vector<string> splitMayPay = parseBetween(s, "pay(", ")", true);
    if(splitMayPay.size())
    {
        GenericPaidAbility * a = NEW GenericPaidAbility(observer, id, card, target,newName,castRestriction,splitMayPay[1],storedPayString,asAlternate);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    //When...comes into play, you may...
    //When...comes into play, choose one...
    const string mayKeywords[] = {"may ", "choice "};
    const bool mayMust[] = { false, true };
    for (size_t i =0; i < sizeof(mayMust)/sizeof(mayMust[0]); ++i)
    {
        if (sWithoutTc.find(mayKeywords[i]) == 0)
        {
            string s1 = sWithoutTc.substr(mayKeywords[i].length());
            MTGAbility * a1 = parseMagicLine(s1, id, spell, card);
            if (!a1)
                return NULL;

            if (tc)
                a1 = NEW GenericTargetAbility(observer, newName,castRestriction,id, card, tc, a1);
            else
                a1 = NEW GenericActivatedAbility(observer, newName,castRestriction,id, card, a1, NULL);
            MayAbility * mainAbility = NEW MayAbility(observer, id, a1, card,mayMust[i],castRestriction);
            return mainAbility;
        }
    }
    // Generic "Until end of turn" effect
    if (s.find("ueot ") == 0)
    {
        string s1 = s.substr(5);
        MTGAbility * a1 = parseMagicLine(s1, id, spell, card);
        if (!a1)
            return NULL;

        return NEW GenericInstantAbility(observer, 1, card, (Damageable *) target, a1);
    }

    //add an ability to the game, this is the "addToGame" version of the above ability.
    if (s.find("activate ") == 0 || s.find(" activate ") == 0)
    {
        string s1 = s.substr(9);
        MTGAbility * a1 = parseMagicLine(s1, id, spell, card);
        if (!a1)
            return NULL;

        return NEW GenericAddToGame(observer,1, card, (Damageable *) target,a1);
    }
    
        // neverending effect
    if (s.find("emblem ") == 0)
    {
        string s1 = s.substr(7);
        MTGAbility * a1 = parseMagicLine(s1, id, spell, card);
        if (!a1)
            return NULL;

        return NEW GenericAbilityMod(observer, 1, card->controller()->getObserver()->ExtraRules,card->controller()->getObserver()->ExtraRules, a1);
    }

    //choose a color
    vector<string> splitChooseAColor = parseBetween(s, "activatechooseacolor ", " activatechooseend");
    if (splitChooseAColor.size())
    {
        return parseChooseActionAbility(unchangedS,card,spell,target,restrictions,id);
    }

    //choose a type
    vector<string> splitChooseAType = parseBetween(s, "activatechooseatype ", " activatechooseend");
    if (splitChooseAType.size())
    {
        return parseChooseActionAbility(unchangedS,card,spell,target,restrictions,id);
    }

    //Upkeep Cost
    found = s.find("upcostmulti");
    if (found != string::npos)
    {
        return AbilityFactory::parseUpkeepAbility(s,card,spell,restrictions,id);
    }
    //Phase based actions
    found = s.find("phaseactionmulti");
    if (found != string::npos)
    {
        return parsePhaseActionAbility(s,card,spell,target,restrictions,id);
    }

    int forcedalive = 0;
    //force an ability to ignore destroy while source is still valid.
    //allow the lords to remove the ability instead of gameobserver checks.
    found = s.find("forcedalive");
    if (found != string::npos)
        forcedalive = 1;
    bool neverRemove = false;
    found = s.find("dontremove");
    if (found != string::npos)
        neverRemove = true;
    //rather dirty way to stop thises and lords from conflicting with each other.
    size_t lord = string::npos;
    for (size_t j = 0; j < kLordKeywordsCount; ++j)
    {
        size_t found2 = s.find(kLordKeywords[j]);
        if (found2 != string::npos && ((found == string::npos) || found2 < found))
        {
            lord = found2;
        }
    }

    //This, ThisForEach;
    found = string::npos;
    int i = -1;
    for (size_t j = 0; j < kThisKeywordsCount; ++j)
    {
        size_t found2 = s.find(kThisKeywords[j]);
        if (found2 != string::npos && ((found == string::npos) || found2 < found))
        {
            found = found2;
            i = j;
        }
    }
    if (found != string::npos && found < lord)
    {
        //why does tc even exist here? This shouldn't happen...
        SAFE_DELETE(tc); //http://code.google.com/p/wagic/issues/detail?id=424

        size_t header = kThisKeywords[i].size();
        size_t end = s.find(")", found + header);
        string s1;
        if (found == 0 || end != s.size() - 1)
        {
            s1 = s.substr(end + 1);
        }
        else
        {
            s1 = s.substr(0, found);
        }
        if (end != string::npos)
        {
            ThisDescriptor * td = NULL;
            string thisDescriptorString = s.substr(found + header, end - found - header);
            vector<string> splitRest = parseBetween(s, "restriction{", "}");
            if (splitRest.size())
            {


            }
            else
            {
                ThisDescriptorFactory tdf;
                td = tdf.createThisDescriptor(observer, thisDescriptorString);

                if (!td)
                {
                    DebugTrace("MTGABILITY: Parsing Error:" << s);
                    return NULL;
                }
            }

            MTGAbility * a = parseMagicLine(s1, id, spell, card, 0, activated);
            if (!a)
            {
                SAFE_DELETE(td);
                return NULL;
            }

            MTGAbility * result = NULL;
            bool oneShot = false;
            found = s.find(" oneshot");
            if (found != string::npos || activated ||
                card->hasType(Subtypes::TYPE_SORCERY) ||
                card->hasType(Subtypes::TYPE_INSTANT) ||
                a->oneShot)
            {
                oneShot = true;
            }
            found = s.find("while ");
            if (found != string::npos)
                oneShot = false;

            Damageable * _target = NULL;
            if (spell)
                _target = spell->getNextDamageableTarget();
            if (!_target)
                _target = target;

            switch (i)
            {
            case 0:
                result = NEW AThis(observer, id, card, _target, td, a);
                break;
            case 1:
                result = NEW AThisForEach(observer, id, card, _target, td, a);
                break;
            case 2:
                result = NEW AThis(observer, id, card, _target, NULL, a, thisDescriptorString);
                break;
            default:
                result = NULL;
            }
            if (result)
            {
                result->oneShot = oneShot;
                a->forcedAlive = forcedalive;
                if(neverRemove)
                {
                    result->forceDestroy = -1;
                    result->forcedAlive = 1;
                    a->forceDestroy = -1;
                    a->forcedAlive = 1;
                }

            }
            return result;
        }
        return NULL;
    }

    //Multiple abilities for ONE cost
    found = s.find("&&");
    if (found != string::npos)
    {
        SAFE_DELETE(tc);
        vector<string> multiEffects = split(s,'&');
        MultiAbility * multi = NEW MultiAbility(observer, id, card, target, NULL);
        for(unsigned int i = 0;i < multiEffects.size();i++)
        {
            if(!multiEffects[i].empty())
            {
                MTGAbility * addAbility = parseMagicLine(multiEffects[i], id, spell, card, activated);
                multi->Add(addAbility);
            }
        }
        multi->oneShot = 1;
        return multi;
    }


    //Lord, foreach, aslongas

    found = string::npos;
    i = -1;
    for (size_t j = 0; j < kLordKeywordsCount; ++j)
    {
        size_t found2 = s.find(kLordKeywords[j]);
        if (found2 != string::npos && ((found == string::npos) || found2 < found))
        {
            found = found2;
            i = j;
        }
    }
    if (found != string::npos)
    {
        SAFE_DELETE(tc);
        size_t header = kLordKeywords[i].size();
        size_t end = s.find(")", found + header);
        string s1;
        if (found == 0 || end != s.size() - 1)
        {
            s1 = s.substr(end + 1);
        }
        else
        {
            s1 = s.substr(0, found);
        }
        if (end != string::npos)
        {
            int lordIncludeSelf = 1;
            size_t other = s1.find(" other");
            if (other != string::npos)
            {
                lordIncludeSelf = 0;
                s1.replace(other, 6, "");
            }
            string lordTargetsString = s.substr(found + header, end - found - header);
            TargetChooserFactory tcf(observer);
            TargetChooser * lordTargets = tcf.createTargetChooser(lordTargetsString, card);

            if (!lordTargets)
            {
                DebugTrace("MTGABILITY: Parsing Error: " << s);
                return NULL;
            }

            MTGAbility * a = parseMagicLine(s1, id, spell, card, false, activated); //activated lords usually force an end of turn ability
            if (!a)
            {
                SAFE_DELETE(lordTargets);
                return NULL;
            }
            MTGAbility * result = NULL;
            bool oneShot = false;
            if (activated || i == 4 || a->oneShot)
                oneShot = true;
            if (card->hasType(Subtypes::TYPE_SORCERY) || card->hasType(Subtypes::TYPE_INSTANT))
                oneShot = true;
            found = s.find("while ");
            if (found != string::npos)
                oneShot = false;
            found = s.find(" oneshot");
            if (found != string::npos)
                oneShot = true;
            Damageable * _target = NULL;
            if (spell)
                _target = spell->getNextDamageableTarget();
            if (!_target)
                _target = target;

            int mini = 0;
            int maxi = 0;
            bool miniFound = false;
            bool maxiFound = false;
            bool compareZone = false;

            found = s.find(" >");
            if (found != string::npos)
            {
                mini = atoi(s.substr(found + 2, 3).c_str());
                miniFound = true;
            }

            found = s.find(" <");
            if (found != string::npos)
            {
                maxi = atoi(s.substr(found + 2, 3).c_str());
                maxiFound = true;
            }
            
            found = s.find("compare");
            if (found != string::npos)
            {
                compareZone = true;
            }
            
            switch (i)
            {
            case 0:
                result = NEW ALord(observer, id, card, lordTargets, lordIncludeSelf, a);
                break;
            case 1:
                result = NEW AForeach(observer, id, card, _target, lordTargets, lordIncludeSelf, a, mini, maxi);
                break;
            case 2:
                {
                    if (!miniFound && !maxiFound)//for code without an operator treat as a mini.
                    {
                        miniFound = true;
                    }
                    result = NEW AAsLongAs(observer, id, card, _target, lordTargets, lordIncludeSelf, a, mini, maxi, miniFound, maxiFound, compareZone);
                }
                break;
            case 3:
                result = NEW ATeach(observer, id, card, lordTargets, lordIncludeSelf, a);
                break;
            case 4:
                result = NEW ALord(observer, id, card, lordTargets, lordIncludeSelf, a);
                break;
            default:
                result = NULL;
            }
            if (result)
            {
                result->oneShot = oneShot;
                a->forcedAlive = forcedalive;
                if (neverRemove)
                {
                    result->oneShot = false;
                    result->forceDestroy = -1;
                    result->forcedAlive = 1;
                    a->forceDestroy = -1;
                    a->forcedAlive = 1;
                }
            }
            return result;
        }
        return NULL;
    }

    //soulbond lord style ability.
    found = s.find("soulbond ");
    if (found != string::npos)
    {
        string s1 = s.substr(found + 9);
        MTGAbility * a = parseMagicLine(s1, id, spell, card, false, activated);
        if(a)
            return NEW APaired(observer,id, card,card->myPair,a);
        return NULL;
    }

    //mana of the listed type doesnt get emptied from the pools.
    vector<string>colorType = parseBetween(s,"poolsave(",")",false);
    if (colorType.size())
    {
        return NEW AManaPoolSaver(observer, id, card, colorType[1], s.find("opponentpool")!=string::npos, s.find("terminate") != string::npos);// Added a way to terminate effect when source card leave battlefield.
    }

    //opponent replace draw with
    found = s.find("opponentreplacedraw ");
    if (found != string::npos)
    {
        string s1 = s.substr(found + 19);
        MTGAbility * a = NULL;
        a = parseMagicLine(s1, id, spell, card, false, activated);
        if(a)
            return NEW ADrawReplacer(observer,id, card,a,true);
        return NULL;
    }

    //replace draw with
    found = s.find("replacedraw ");
    if (found != string::npos)
    {
        string s1 = s.substr(found + 11);
        MTGAbility * a = NULL;
        a = parseMagicLine(s1, id, spell, card, false, activated);
        if(a)
            return NEW ADrawReplacer(observer,id, card,a);
        return NULL;
    }

    if (!activated && tc)
    {
        MTGAbility * a = parseMagicLine(sWithoutTc, id, spell, card);
        if (!a)
        {
            DebugTrace("ABILITYFACTORY Error parsing: " << s);
            return NULL;
        }
        a = NEW GenericTargetAbility(observer, newName,castRestriction,id, card, tc, a);
        return NEW MayAbility(observer, id, a, card, true);
    }

    SAFE_DELETE(tc);
    
    //dynamic ability builder
    vector<string> splitDynamic = parseBetween(s, "dynamicability<!", "!>");
    if (splitDynamic.size())
    {
        string s1 = splitDynamic[1];
        int type = 0;
        int effect = 0;
        int who = 0;
        int amountsource = 0;

        //source
        for (size_t i = 0; i < sizeof(kDynamicSourceIds)/sizeof(kDynamicSourceIds[0]); ++i)
        {
            size_t abilityamountsource = s1.find(kDynamicSourceKeywords[i]);
            if (abilityamountsource != string::npos)
            {
                amountsource = kDynamicSourceIds[i];
                break;
            }
        }

        //main variable or type
        for (size_t i = 0; i < sizeof(kDynamicTypeIds)/sizeof(kDynamicTypeIds[0]); ++i)
        {
            size_t abilitytype = s1.find(kDynamicTypeKeywords[i]);
            if (abilitytype != string::npos)
            {
                type = kDynamicTypeIds[i];
                break;
            }
        }

        //effect
        for (size_t i = 0; i < sizeof(kDynamicEffectIds)/sizeof(kDynamicEffectIds[0]); ++i)
        {
            size_t abilityeffect = s1.find(kDynamicEffectKeywords[i]);
            if (abilityeffect != string::npos)
            {
                effect = kDynamicEffectIds[i];
                break;
            }
        }

        //target
        for (size_t i = 0; i < sizeof(kDynamicWhoIds)/sizeof(kDynamicWhoIds[0]); ++i)
        {
            size_t abilitywho = s1.find(kDynamicWhoKeywords[i]);
            if (abilitywho != string::npos)
            {
                who = kDynamicWhoIds[i];
                break;
            }
        }

        string sAbility = splitDynamic[2];
        MTGAbility * stored = NULL;
        if(!sAbility.empty())
            stored = parseMagicLine(sAbility, id, spell, card);

        MTGAbility * a = NEW AADynamic(observer, id, card, target,type,effect,who,amountsource,stored);
        a->oneShot = 1;
        return a;
   }

    //flip a coin
    vector<string> splitFlipCoin = parseBetween(s, "flipacoin ", " flipend");
    if (splitFlipCoin.size())
    {
        string a1 = splitFlipCoin[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            userchoice = (a1[0] - 48);
            a1 = a1.substr(2);
        }
        MTGAbility * a = NEW GenericFlipACoin(observer, id, card, target, a1, NULL, userchoice);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //roll a d4 die
    vector<string> splitRollD4 = parseBetween(s, "rolld4 ", " rolld4end");
    if (splitRollD4.size())
    {
        string a1 = splitRollD4[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            userchoice = (a1[0] - 48);
            a1 = a1.substr(2);
        }
        MTGAbility * a = NEW GenericRollDie(observer, id, card, target, a1, NULL, userchoice, 4);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //roll a d6 die
    vector<string> splitRollD6 = parseBetween(s, "rolld6 ", " rolld6end");
    if (splitRollD6.size())
    {
        string a1 = splitRollD6[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            userchoice = (a1[0] - 48);
            a1 = a1.substr(2);
        }
        MTGAbility * a = NEW GenericRollDie(observer, id, card, target, a1, NULL, userchoice, 6);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //roll a d8 die
    vector<string> splitRollD8 = parseBetween(s, "rolld8 ", " rolld8end");
    if (splitRollD8.size())
    {
        string a1 = splitRollD8[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            userchoice = (a1[0] - 48);
            a1 = a1.substr(2);
        }
        MTGAbility * a = NEW GenericRollDie(observer, id, card, target, a1, NULL, userchoice, 8);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //roll a d10 die
    vector<string> splitRollD10 = parseBetween(s, "rolld10 ", " rolld10end");
    if (splitRollD10.size())
    {
        string a1 = splitRollD10[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            if(a1[1] >= 48 && a1[1] <= 57){
                userchoice = (a1[0] - 48)*10 + (a1[1] - 48);
                a1 = a1.substr(3);
            } else {
                userchoice = (a1[0] - 48);
                a1 = a1.substr(2);
            }
        }
        MTGAbility * a = NEW GenericRollDie(observer, id, card, target, a1, NULL, userchoice, 10);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //roll a d12 die
    vector<string> splitRollD12 = parseBetween(s, "rolld12 ", " rolld12end");
    if (splitRollD12.size())
    {
        string a1 = splitRollD12[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            if(a1[1] >= 48 && a1[1] <= 57){
                userchoice = (a1[0] - 48)*10 + (a1[1] - 48);
                a1 = a1.substr(3);
            } else {
                userchoice = (a1[0] - 48);
                a1 = a1.substr(2);
            }
        }
        MTGAbility * a = NEW GenericRollDie(observer, id, card, target, a1, NULL, userchoice, 12);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //roll a d20 die
    vector<string> splitRollD20 = parseBetween(s, "rolld20 ", " rolld20end");
    if (splitRollD20.size())
    {
        string a1 = splitRollD20[1];
        int userchoice = 0;
        if(a1[0] >= 48 && a1[0] <= 57){
            if(a1[1] >= 48 && a1[1] <= 57){
                userchoice = (a1[0] - 48)*10 + (a1[1] - 48);
                a1 = a1.substr(3);
            } else {
                userchoice = (a1[0] - 48);
                a1 = a1.substr(2);
            }
        }
        MTGAbility * a = NEW GenericRollDie(observer, id, card, target, a1, NULL, userchoice, 20);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //Phase based actions  
    found = s.find("phaseaction");
    if (found != string::npos)
    {
        return parsePhaseActionAbility(s,card,spell,target,restrictions,id);
    }

    //may pay ability
    vector<string> splitMayPaysub = parseBetween(s, "pay[[","]]", true);
    if (splitMayPaysub.size())
    {
        GenericPaidAbility * a = NEW GenericPaidAbility(observer, id, card, target,newName,castRestriction,splitMayPaysub[1],storedPayString,asAlternate);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //Upkeep Cost
    found = s.find("upcost");
    if (found != string::npos)
    {
       return AbilityFactory::parseUpkeepAbility(s,card,spell,restrictions,id);
    }

    //ninjutsu
    found = s.find("ninjutsu");
    if (found != string::npos)
    {
        MTGAbility * a = NEW ANinja(observer, id, card, target, true);
        a->oneShot = 1;
        return a;
    }

    //readytofight
    found = s.find("readytofight");
    if (found != string::npos)
    {
        MTGAbility * a = NEW ANinja(observer, id, card, target, false);
        a->oneShot = 1;
        return a;
    }

    //combat removal
    found = s.find("removefromcombat");
    if (found != string::npos)
    {
        MTGAbility * a = NEW ACombatRemoval(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

    //gain control until source is untapped or leaves battlefield
    found = s.find("shackle");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AShackleWrapper(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

    //grant ability until source is untapped or leaves battlefield
    found = s.find("grant ");
    if (found != string::npos)
    {
        MTGAbility * toGrant = parseMagicLine(storedAbilityString, id, spell, card);
        MTGAbility * a = NEW AGrantWrapper(observer, id, card, target,toGrant);
        a->oneShot = 1;
        return a;
    }

   //momentary blink
    found = s.find("(blink)");
    if (found != string::npos)
    {
        bool ueoteffect = (s.find("(blink)ueot") != string::npos);
        bool forsource = (s.find("(blink)forsrc") != string::npos);
        bool blinkhand = (s.find("hand(blink)") != string::npos);
        size_t returnAbility = s.find("return(");
        string sAbility = s.substr(returnAbility + 7);
        MTGAbility * stored = NULL;
        if(!sAbility.empty())
        {
            stored = parseMagicLine(sAbility, id, spell, card);
        }
        MTGAbility * a = NEW ABlinkGeneric(observer, id, card, target,ueoteffect,forsource,blinkhand,stored);
        a->oneShot = 1;
        return a;
    }

    // Fizzle (counterspell...) and put to zone or Move spell to zone
    // This should always be above "fizzle" section
    vector<string> splitFizzle = parseBetween(s, "spellmover(", ")");
    if (!splitFizzle.size())
        splitFizzle = parseBetween(s, "fizzleto(", ")");
    if (splitFizzle.size())
    {
        // currently only hand, exile and library are supported
        string zone = splitFizzle[1];
        ActionStack::FizzleMode fizzleMode = ActionStack::PUT_IN_GRAVEARD;
        if (zone == "hand")
        {
            fizzleMode = ActionStack::PUT_IN_HAND;
        } else if (zone == "exile")
        {
            fizzleMode = ActionStack::PUT_IN_EXILE;
        }  else if (zone == "exileimp")
        {
            fizzleMode = ActionStack::PUT_IN_EXILE_IMPRINT;
        } else if (zone == "librarytop")
        {
            fizzleMode = ActionStack::PUT_IN_LIBRARY_TOP;
        } else if (zone == "librarysecond")
        {
            fizzleMode = ActionStack::PUT_IN_LIBRARY_SECOND;
        } else if (zone == "librarybottom")
        {
            fizzleMode = ActionStack::PUT_IN_LIBRARY_BOTTOM;
        }
        Spell * starget = NULL;
        if (spell)
            starget = spell->getNextSpellTarget();
        AAFizzler * a = NEW AAFizzler(observer, id, card, starget);
        a->fizzleMode = fizzleMode;
        a->spellMover = (s.find("spellmover(") != string::npos);
        a->oneShot = 1;
        return a;
    }

    //Fizzle (counterspell...)
    found = s.find("fizzle");
    if (found != string::npos && s.find("nofizzle") == string::npos) //Fix to allow adding nofizzle ability to cards which originally have not (e.g. with transforms or lord keywords)
    {
        Spell * starget = NULL;
        if (spell)
            starget = spell->getNextSpellTarget();
        MTGAbility * a = NEW AAFizzler(observer, id, card, starget);
        a->oneShot = 1;
        return a;
    }

    //Describes a player target in many abilities
    int who = TargetChooser::UNSET;
    if (s.find(" controller") != string::npos)
        who = TargetChooser::CONTROLLER;
    if (s.find(" opponent") != string::npos)
        who = TargetChooser::OPPONENT;
    if (s.find(" targetcontroller") != string::npos)
        who = TargetChooser::TARGET_CONTROLLER;
    if (s.find(" targetedplayer") != string::npos)
        who = TargetChooser::TARGETED_PLAYER;
    if (s.find(" owner") != string::npos)
        who = TargetChooser::OWNER;

    //ability creator the target has to do the ability.
    if(s.find("ability$") != string::npos)
    {
        if (storedAbilityString.size())
        {
            vector<string> splitName = parseBetween(storedAbilityString, "name(", ")");
            if (splitName.size())
            {
                newName = splitName[1];
                storedAbilityString = splitName[0];
                storedAbilityString.append(splitName[2]);
                //we erase the name section from the string to avoid 
                //accidently building an mtg ability with the text meant for menuText.
            }
            ATargetedAbilityCreator * abl = NEW ATargetedAbilityCreator(observer, id, card,target, NULL,newName, storedAbilityString, who);
            abl->oneShot = 1;
            storedString.clear();
            storedAbilityString.clear();
            return abl;
        }
    }
    //livingweapon (used for token below)
    bool aLivingWeapon = (s.find("livingweapon") != string::npos);

    //Token creator. Name, type, p/t, abilities
    vector<string> splitToken = parseBetween(s, "token(", ")");
    if (splitToken.size())
    {
        WParsedInt * multiplier = NULL;
        size_t star = s.find("*");
        string starfound = "";
        if (star != string::npos)
        {
            starfound = s.substr(star + 1);
            size_t starEnd= starfound.find_first_of(" ");
            starfound = starfound.substr(0,starEnd);
            multiplier = NEW WParsedInt(starfound, spell, card);
        }
        replace(splitToken[1].begin(), splitToken[1].end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using token keyword inside transforms)
        int tokenId = atoi(splitToken[1].c_str());
        MTGCardInstance * creator = NULL;
        if(card  && card->name.empty() && card->storedSourceCard) // Fix for token creation inside ability$!!$ keyword.
            creator = card->storedSourceCard;
        else
            creator = card;
        if (tokenId)
        {
            MTGCard * safetycard = MTGCollection()->getCardById(tokenId);
            if (!safetycard) //Error, card not foudn in DB
                return NEW ATokenCreator(observer, id, creator, target, NULL, "ID NOT FOUND", "ERROR ID", 0, 0, "", "", NULL, 0);

            ATokenCreator * tok = NEW ATokenCreator(observer, id, creator, target, NULL, tokenId, starfound, multiplier, who);
            tok->oneShot = 1;
            //andability
            if(storedAndAbility.size())
            {
                string stored = storedAndAbility;
                storedAndAbility.clear();
                ((ATokenCreator*)tok)->andAbility = parseMagicLine(stored, id, spell, card);
            }
            return tok;
        }
        string tokenDesc = splitToken[1];
        vector<string> tokenParameters = split(tokenDesc, ',');
        string sabilities = "";
        //lets try finding a token by card name.
        if (splitToken[1].size() && (tokenParameters.size() ==1||tokenParameters.size() ==2))
        {
            string cardName = splitToken[1];
            size_t triggerpos = cardName.find(",notrigger");
            if(triggerpos != string::npos){
                cardName.replace(triggerpos, 10, ""); // To allow the usage of notrigger even with named token (e.g. Academy Manufactor)
                sabilities = "notrigger";
            }
            MTGCard * safetycard = MTGCollection()->getCardByName(cardName);
            if (safetycard) //lets try constructing it then,we didnt find it by name
            {
                ATokenCreator * tok = NEW ATokenCreator(observer, id, creator, target, NULL, cardName, starfound, multiplier, who);
                tok->oneShot = 1;
                if(!sabilities.empty())
                    tok->sabilities = sabilities;
                //andability
                if(storedAndAbility.size())
                {
                    string stored = storedAndAbility;
                    storedAndAbility.clear();
                    ((ATokenCreator*)tok)->andAbility = parseMagicLine(stored, id, spell, card);
                }
                return tok;
            }
        }
        if (tokenParameters.size() < 3)
        {
            DebugTrace("incorrect Parameters for Token" << tokenDesc);
            return NULL;
        }
        string sname = tokenParameters[0];
        string stypes = tokenParameters[1];
        string spt = tokenParameters[2];
        string cID = "";
        //reconstructing string abilities from the split version,
        // then we re-split it again in the token constructor,
        // this needs to be improved
        sabilities = (tokenParameters.size() > 3)? tokenParameters[3] : "";
        for (size_t i = 4; i < tokenParameters.size(); ++i)
        {
            sabilities.append(",");
            sabilities.append(tokenParameters[i]);
        }
        if(sabilities.find(",tnum.") != string::npos)
        {
            size_t begins = sabilities.find(",tnum.");
            cID = sabilities.substr(begins+6);
            sabilities = cReplaceString(sabilities,",tnum."+cID,"");
        }
        int value = 0;
        if (spt.find("xx/xx") != string::npos)
            value = card->X / 2;
        else if (spt.find("x/x") != string::npos)
            value = card->X;

        int power, toughness;
        parsePowerToughness(spt, &power, &toughness);
        
        ATokenCreator * tok = NEW ATokenCreator(
            observer, id, creator, target, NULL, sname, stypes, power + value, toughness + value,
            sabilities, starfound, multiplier, who, aLivingWeapon, spt, cID);
        tok->oneShot = 1;
        if(aLivingWeapon)
            tok->forceDestroy = 1;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((ATokenCreator*)tok)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return tok;  
    }

    //Alternative Token creator. Name, type, p/t, abilities - uses ":" as delimeter
    vector<string> makeToken = parseBetween(s, "create(", ")");
    if (makeToken.size())
    {
        WParsedInt * multiplier = NULL;
        size_t myMultiplier = s.find("*");
        string myMultiplierfound = "";
        if (myMultiplier != string::npos)
        {
            myMultiplierfound = s.substr(myMultiplier + 1);
            size_t myMultiplierEnd= myMultiplierfound.find_first_of(" ");
            myMultiplierfound = myMultiplierfound.substr(0,myMultiplierEnd);
            multiplier = NEW WParsedInt(myMultiplierfound, spell, card);
        }

        MTGCardInstance * creator = NULL;
        if(card  && card->name.empty() && card->storedSourceCard) // Fix for token creation inside ability$!!$ keyword.
            creator = card->storedSourceCard;
        else
            creator = card;

        int mytokenId = atoi(makeToken[1].c_str());
        if (mytokenId)
        {
            MTGCard * mysafetycard = MTGCollection()->getCardById(mytokenId);
            if (!mysafetycard) //Error, card not foudn in DB
                return NEW ATokenCreator(observer, id, creator, target, NULL, "ID NOT FOUND", "ERROR ID", 0, 0, "", "", NULL, 0);

            ATokenCreator * mtok = NEW ATokenCreator(observer, id, creator, target, NULL, mytokenId, myMultiplierfound, multiplier, who);
            mtok->oneShot = 1;
            //andability
            if(storedAndAbility.size())
            {
                string stored = storedAndAbility;
                storedAndAbility.clear();
                ((ATokenCreator*)mtok)->andAbility = parseMagicLine(stored, id, spell, card);
            }
            return mtok;
        }
        
        string tokenDesc = makeToken[1];
        vector<string> tokenParameters = split(tokenDesc, ':');
        //lets try finding a token by card name.
        if (makeToken[1].size() && tokenParameters.size() ==1)
        {
            string cardName = makeToken[1];
            MTGCard * mysafetycard = MTGCollection()->getCardByName(cardName);
            if (mysafetycard) //lets try constructing it then,we didnt find it by name
            {
                ATokenCreator * mtok = NEW ATokenCreator(observer, id, creator, target, NULL, cardName, myMultiplierfound, multiplier, who);
                mtok->oneShot = 1;
                //andability
                if(storedAndAbility.size())
                {
                    string stored = storedAndAbility;
                    storedAndAbility.clear();
                    ((ATokenCreator*)mtok)->andAbility = parseMagicLine(stored, id, spell, card);
                }
                return mtok;
            }
        }
        if (tokenParameters.size() < 3)
        {
            DebugTrace("incorrect Parameters for Token" << tokenDesc);
            return NULL;
        }
        string sname = tokenParameters[0];
        string stypes = tokenParameters[1];
        string spt = tokenParameters[2];
        string cID = "";
        //reconstructing string abilities from the split version,
        // then we re-split it again in the token constructor,
        // this needs to be improved
        string sabilities = (tokenParameters.size() > 3)? tokenParameters[3] : "";
        for (size_t i = 4; i < tokenParameters.size(); ++i)
        {
            sabilities.append(",");
            sabilities.append(tokenParameters[i]);
        }
        if(sabilities.find(",tnum.") != string::npos)
        {
            size_t begins = sabilities.find(",tnum.");
            cID = sabilities.substr(begins+6);
            sabilities = cReplaceString(sabilities,",tnum."+cID,"");
        }
        int value = 0;
        if (spt.find("xx/xx") != string::npos)
            value = card->X / 2;
        else if (spt.find("x/x") != string::npos)
            value = card->X;

        int power, toughness;
        parsePowerToughness(spt, &power, &toughness);
        
        ATokenCreator * mtok = NEW ATokenCreator(
            observer, id, creator, target, NULL, sname, stypes, power + value, toughness + value,
            sabilities, myMultiplierfound, multiplier, who, aLivingWeapon, spt, cID);
        mtok->oneShot = 1;
        if(aLivingWeapon)
            mtok->forceDestroy = 1;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((ATokenCreator*)mtok)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return mtok;  
    }

    //Equipment
    found = s.find("equip");
    if (found != string::npos)
    {
        if ((s.find("equipment") == string::npos) && (s.find("equipped") == string::npos)) // Fix a bug on parser when reading the substring "equip" with a different meaning.
            return NEW AEquip(observer, id, card);
    }

    //Reconfiguration
    found = s.find("reconfigure");
    if (found != string::npos)
    {
        AEquip* a = NEW AEquip(observer, id, card);
        a->isReconfiguration = true;
        return a;
    }

    // TODO: deprecate this ability in favor of retarget
    //Equipment (attach)
    found = s.find("attach");
    if (found != string::npos)
    {
        return NEW AEquip(observer, id, card, 0, ActivatedAbility::NO_RESTRICTION);
    }

    //MoveTo Move a card from a zone to another
    vector<string> splitMove = parseBetween(s, "moveto(", ")");
    if (splitMove.size())
    {
        //hack for http://code.google.com/p/wagic/issues/detail?id=120
        //We assume that auras don't move their own target...
        if (card->hasType(Subtypes::TYPE_AURA))
            target = card;

        MTGAbility * a = NEW AAMover(observer, id, card, target, splitMove[1],newName);
        a->oneShot = true;
        ((AAMover*)a)->necro = s.find("hiddenmoveto") != string::npos?true:false;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAMover*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //random mover
     vector<string> splitRandomMove = parseBetween(s, "moverandom(", ")");
    if (splitRandomMove.size())
    {
         vector<string> splitfrom = parseBetween(splitRandomMove[2], "from(", ")");
         vector<string> splitto = parseBetween(splitRandomMove[2], "to(", ")");
         if(!splitfrom.size() || !splitto.size())
             return NULL;
         MTGAbility * a = NEW AARandomMover(observer, id, card, target, splitRandomMove[1],splitfrom[1],splitto[1]);
         a->oneShot = true;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AARandomMover*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
         return a;
    }

    //put a card in a specifc position of owners library from the top
    vector<string> splitPlaceFromTop = parseBetween(s, "placefromthetop(", ")");
    if (splitPlaceFromTop.size())
    {
        int position = 0;
        WParsedInt* parser = NEW WParsedInt(splitPlaceFromTop[1], card);
        if(parser){
            position = parser->intValue;
            SAFE_DELETE(parser);
        }
        MTGAbility * a = NEW AALibraryPosition(observer, id, card, target, NULL, position);
        a->oneShot = 1;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AALibraryBottom*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //put a card on bottom of library
    found = s.find("bottomoflibrary");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AALibraryBottom(observer, id, card, target);
        a->oneShot = 1;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AALibraryBottom*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Copy a target
    found = s.find("copy");
    if (found != string::npos)
    {
        string options = "";
        vector<string> splitOptions = parseBetween(s, "options(", ")");
        if (splitOptions.size())
        {
            options = splitOptions[1];
        }
        MTGAbility * a = NEW AACopier(observer, id, card, target, NULL, options);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        ((AACopier*)a)->isactivated = activated;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AACopier*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //imprint
    found = s.find("imprint");
    if (found != string::npos)
    {
        if (s.find("imprintedcard") == string::npos)
        {
            MTGAbility * a = NEW AAImprint(observer, id, card, target);
            a->oneShot = 1;
            //andability
            if(storedAndAbility.size())
            {
                string stored = storedAndAbility;
                storedAndAbility.clear();
                ((AAImprint*)a)->andAbility = parseMagicLine(stored, id, spell, card);
            }
            return a;
        }
    }

    //haunt a creature
    found = s.find("haunt");
    if (found != string::npos)
    {
        if (s.find("haunted") == string::npos)
        {
            MTGAbility * a = NEW AAHaunt(observer, id, card, target);
            a->oneShot = 1;
            //andability
            if(storedAndAbility.size())
            {
                string stored = storedAndAbility;
                storedAndAbility.clear();
                ((AAHaunt*)a)->andAbility = parseMagicLine(stored, id, spell, card);
            }
            return a;
        }
    }

    //train a creature
    found = s.find("dotrain");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AATrain(observer, id, card, target);
        a->oneShot = 1;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AATrain*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Conjure a card
    found = s.find("conjure");
    if (found != string::npos)
    {
        replace(s.begin(), s.end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using conjure keyword inside transforms)
        string cardName = "";
        vector<string> splitCard = parseBetween(s, "cards(", ")");
        if (splitCard.size())
        {
            cardName = splitCard[1];
        }
        if(cardName == "myname")
            cardName = card->name; // Added to refer the orginal source card name (e.g. "Clone Crafter").
        string cardZone = "";
        vector<string> splitZone = parseBetween(s, "zone(", ")");
        if (splitZone.size())
        {
            cardZone = splitZone[1];
        }
        MTGAbility * a = NEW AAConjure(observer, id, card, target, NULL, cardName, cardZone);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAConjure*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //foretell
    found = s.find("doforetell");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAForetell(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

    //phaseout
    found = s.find("phaseout");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAPhaseOut(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

    //manifest
    found = s.find("manifest");
    if (found != string::npos)
    {//for cloudform, rageform and lightform
        bool withenchant = s.find("withenchant") != string::npos;
        MTGAbility * a = NEW AManifest(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AManifest*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        if(withenchant)
            ((AManifest*)a)->withenchant = true;
        return a;
    }
    //exert
    found = s.find("exert");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AExert(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AExert*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }
    //provoke
    found = s.find("provoke");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AProvoke(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AProvoke*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }
    //setblocker
    found = s.find("setblocker");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AProvoke(observer, id, card, target);
        a->oneShot = 1;
        ((AProvoke*)a)->setblocker = true;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AProvoke*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //clone
    found = s.find("clone");
    if (found != string::npos)
    {
        string with = "";
        string types = "";
        string options = "";
        replace(s.begin(), s.end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using clone keyword inside transforms)
        vector<string> splitWith = parseBetween(s, "with(", ")");
        if (splitWith.size())
        {
            with = splitWith[1];
        }
        vector<string> splitTypes = parseBetween(s, "addtype(", ")");
        if (splitTypes.size())
        {
            types = splitTypes[1];
        }
        vector<string> splitOptions = parseBetween(s, "options(", ")");
        if (splitOptions.size())
        {
            options = splitOptions[1];
        }
        MTGAbility * a = NEW AACloner(observer, id, card, target, 0, who, with, types, options);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AACloner*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Bury, destroy, sacrifice, reject(discard)
    if (s.find("bury") != string::npos)
    {
        MTGAbility *a = NEW AABuryCard(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AABuryCard*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    if (s.find("destroy") != string::npos)
    {
        MTGAbility * a = NEW AADestroyCard(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AADestroyCard*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    if (s.find("sacrifice") != string::npos || s.find("exploits") != string::npos)
    {
        MTGAbility *a = NEW AASacrificeCard(observer, id, card, target);
        a->oneShot = 1;
        ((AASacrificeCard*)a)->isExploited = (s.find("exploits") != string::npos)?true:false; // added to allow the Exploit trigger.
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AASacrificeCard*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    if (s.find("reject") != string::npos)
    {
        MTGAbility *a = NEW AADiscardCard(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AADiscardCard*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //cast a card without paying it's manacost
    vector<string> splitCastCard = parseBetween(s, "castcard(", ")");
    if (splitCastCard.size())
    {
        string builtHow = splitCastCard[1];
        bool withRestrictions = splitCastCard[1].find("restricted") != string::npos;
        bool asCopy = splitCastCard[1].find("copied") != string::npos;
        bool asNormal = splitCastCard[1].find("normal") != string::npos;
        bool asNormalMadness = splitCastCard[1].find("madness") != string::npos;
        bool sendNoEvent = splitCastCard[1].find("noevent") != string::npos;
        bool putinplay = splitCastCard[1].find("putinplay") != string::npos;
        bool alternative = splitCastCard[1].find("alternative") != string::npos;
        bool flashback = splitCastCard[1].find("flashback") != string::npos;
        bool flipped = splitCastCard[1].find("flipped") != string::npos;
        string nameCard = "";
        if(splitCastCard[1].find("named!:") != string::npos)
        {
            vector<string> splitCastName = parseBetween(splitCastCard[1], "named!:", ":!");
            if(splitCastName.size())
            {
                nameCard = splitCastName[1];
            }
        }
        int kicked = 0;
        if(splitCastCard[1].find("kicked!:") != string::npos)
        {
            vector<string> splitCastKicked = parseBetween(splitCastCard[1], "kicked!:", ":!");
            if(splitCastKicked.size())
            {
                WParsedInt * val = NEW WParsedInt(splitCastKicked[1], NULL, card);
                kicked = val->getValue();
            }
        }
        int costx = 0;
        if(splitCastCard[1].find("costx!:") != string::npos)
        {
            vector<string> splitCastCostX = parseBetween(splitCastCard[1], "costx!:", ":!");
            if(splitCastCostX.size())
            {
                WParsedInt * val = NEW WParsedInt(splitCastCostX[1], NULL, card);
                costx = val->getValue();
            }
        }
        MTGAbility *a = NEW AACastCard(observer, id, card, target, withRestrictions, asCopy, asNormal, nameCard, newName, sendNoEvent, putinplay, asNormalMadness, alternative, kicked, costx, flipped, flashback);
        a->oneShot = false;
        if(splitCastCard[1].find("trigger[to]") != string::npos)
        {
            a->setActionTC(NEW TriggerTargetChooser(observer, WEvent::TARGET_TO));
        }
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AACastCard*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        MTGCardInstance * _target = NULL;
        if (spell)
            _target = spell->getNextCardTarget();
        if(!_target)
            _target = target;
        a->target = _target;
        return a;
    }

    bool oneShot = false;
    bool forceForever = false;
    bool untilYourNextTurn = false;
    bool untilYourNextEndTurn = false;
    found = s.find("ueot");
    if (found != string::npos)
        forceUEOT = true;
    found = s.find("oneshot");
    if (found != string::npos)
        oneShot = true;
    found = s.find("forever");
    if (found != string::npos)
        forceForever = true;
    found = s.find("uynt");
    if (found != string::npos)
        untilYourNextTurn = true;
    found = s.find("uent");
    if (found != string::npos)
        untilYourNextEndTurn = true;
    //Prevent Damage
    const string preventDamageKeywords[] = { "preventallcombatdamage", "preventallnoncombatdamage", "preventalldamage", "fog" };
    const int preventDamageTypes[] = {0, 2, 1, 0}; //TODO enum ?
    const bool preventDamageForceOneShot[] = { false, false, false, true };

    for (size_t i = 0; i < sizeof(preventDamageTypes)/sizeof(preventDamageTypes[0]); ++i)
    {
        found = s.find(preventDamageKeywords[i]);
        if (found != string::npos)
        {
            string to = "";
            string from = "";

            vector<string> splitTo = parseBetween(s, "to(", ")");
            if (splitTo.size())
                to = splitTo[1];

            vector<string> splitFrom = parseBetween(s, "from(", ")");
            if (splitFrom.size())
                from = splitFrom[1];

            MTGAbility * ab;
            if (forceUEOT || preventDamageForceOneShot[i])
                ab = NEW APreventDamageTypesUEOT(observer, id, card, to, from, preventDamageTypes[i]);
            else
                ab = NEW APreventDamageTypes(observer, id, card, to, from, preventDamageTypes[i]);

            if (preventDamageForceOneShot[i])
                ab->oneShot = 1;

            return ab;
        }
    }
 
    //Reset damages on cards
    found = s.find("resetdamage");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAResetDamage(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

        //Do nothing
    found = s.find("donothing");
    if (found != string::npos)
    {
        
        MTGAbility * a = NEW AAFakeAbility(observer, id, card, target,newName);
        a->oneShot = 1;
        return a;
    }

        //Epic
    found = s.find("epic");
    if (found != string::npos)
    {
        
        MTGAbility * a = NEW AAEPIC(observer, id, card, target,newName);
        a->oneShot = 1;
        return a;
    }

        //Forcefield
    found = s.find("forcefield");
    if (found != string::npos)
    {
        
        MTGAbility * a = NEW AAEPIC(observer, id, card, target,"Forcefield",NULL,true);
        a->oneShot = 1;
        return a;
    }

    //Damage
    vector<string> splitDamage = parseBetween(s, "damage:", " ", false);
    if (splitDamage.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AADamager(observer, id, card, t, splitDamage[1], NULL, who);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AADamager*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //remove poison
    vector<string> splitPoison = parseBetween(s, "alterpoison:", " ", false);
    if (splitPoison.size())
    {
        int poison = 0;
        WParsedInt* parser = NEW WParsedInt(splitPoison[1], card);
        if(parser){
            poison = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterPoison(observer, id, card, t, poison, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //alter energy
    vector<string> splitEnergy = parseBetween(s, "alterenergy:", " ", false);
    if (splitEnergy.size())
    {
        int energy = 0;
        WParsedInt* parser = NEW WParsedInt(splitEnergy[1], card);
        if(parser){
            energy = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterEnergy(observer, id, card, t, energy, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //alter experience
    vector<string> splitExperience = parseBetween(s, "alterexperience:", " ", false);
    if (splitExperience.size())
    {
        int exp = 0;
        WParsedInt* parser = NEW WParsedInt(splitExperience[1], card);
        if(parser){
            exp = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterExperience(observer, id, card, t, exp, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //alter dungeon completed
    vector<string> splitDungeonCompleted = parseBetween(s, "completedungeon:", " ", false);
    if (splitDungeonCompleted.size())
    {
        int dungeoncompleted = 0;
        WParsedInt* parser = NEW WParsedInt(splitDungeonCompleted[1], card);
        if(parser){
            dungeoncompleted = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterDungeonCompleted(observer, id, card, t, dungeoncompleted, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //alter yidaro counter
    vector<string> splitYidaroCounter = parseBetween(s, "alteryidarocount:", " ", false);
    if (splitYidaroCounter.size())
    {
        int yidarocount = 0;
        WParsedInt* parser = NEW WParsedInt(splitYidaroCounter[1], card);
        if(parser){
            yidarocount = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterYidaroCount(observer, id, card, t, yidarocount, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //Ring temptations
    vector<string> splitRingTemptations = parseBetween(s, "theringtempts:", " ", false);
    if (splitRingTemptations.size())
    {
        int temptations = 1;
        WParsedInt* parser = NEW WParsedInt(splitRingTemptations[1], card);
        if(parser){
            temptations = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterRingTemptations(observer, id, card, t, temptations, NULL, who);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAAlterRingTemptations*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //becomes monarch
    vector<string> splitMonarch = parseBetween(s, "becomesmonarch", " ", false);
    if (splitMonarch.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterMonarch(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //takes the initiative
    vector<string> splitInitiative = parseBetween(s, "taketheinitiative", " ", false);
    if (splitInitiative.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterInitiative(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //alter mutation counter on target card with trigger activation
    vector<string> splitMutated = parseBetween(s, "altermutationcounter:", " ", false);
    if (splitMutated.size())
    {
        WParsedInt* parser = NEW WParsedInt(splitMutated[1], card);
        if(parser){
            card->mutation += parser->intValue;
            SAFE_DELETE(parser);
        }
        WEvent * e = NEW WEventCardMutated(card);
        card->getObserver()->receiveEvent(e);
    }

    //set mutation counter on source card with no trigger activation
    vector<string> splitMutatedOver = parseBetween(s, "mutationover:", " ", false);
    if (splitMutatedOver.size())
    {
        WParsedInt* parser = NEW WParsedInt(splitMutated[1], card);
        if(parser){
            card->mutation += parser->intValue;
            SAFE_DELETE(parser);
        }
    }
    vector<string> splitMutatedUnder = parseBetween(s, "mutationunder:", " ", false);
    if (splitMutatedUnder.size())
    {
        WParsedInt* parser = NEW WParsedInt(splitMutated[1], card);
        if(parser){
            card->mutation += parser->intValue;
            SAFE_DELETE(parser);
        }
    }

    //perform boast
    found = s.find("doboast");
    if (found != string::npos)
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AABoastEvent(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //perform surveil
    found = s.find("surveil");
    if (found != string::npos)
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AASurveilEvent(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //perform explores
    found = s.find("explores");
    if (found != string::npos)
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAExploresEvent(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //becomes the Ring bearer
    found = s.find("becomesringbearer");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AARingBearerChosen(observer, id, card, target, NULL);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AARingBearerChosen*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //set surveil offset of a player (eg. Enhanced Surveillance)
    vector<string> splitSurveilOffset = parseBetween(s, "altersurvoffset:", " ", false);
    if (splitSurveilOffset.size())
    {
        int surveilOffset = 0;
        WParsedInt* parser = NEW WParsedInt(splitSurveilOffset[1], card);
        if(parser){
            surveilOffset = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterSurveilOffset(observer, id, card, t, surveilOffset, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //set devotion offset of a player (eg. Altar of the Pantheon)
    vector<string> splitDevotionOffset = parseBetween(s, "alterdevoffset:", " ", false);
    if (splitDevotionOffset.size())
    {
        int devotionOffset = 0;
        WParsedInt* parser = NEW WParsedInt(splitDevotionOffset[1], card);
        if(parser){
            devotionOffset = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAAlterDevotionOffset(observer, id, card, t, devotionOffset, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //prevent next damage
    vector<string> splitPrevent = parseBetween(s, "prevent:", " ", false);
    if (splitPrevent.size())
    {
        int preventing = 0;
        WParsedInt* parser = NEW WParsedInt(splitPrevent[1], card);
        if(parser){
            preventing = parser->intValue;
            SAFE_DELETE(parser);
        }
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AADamagePrevent(observer, id, card, t, preventing, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //modify hand size - reduce maximum or increase
    vector<string> splitHandMod = parseBetween(s, "hmodifer:", " ", false);
    if (splitHandMod.size())
    {
        Damageable * t = spell ? spell->getNextDamageableTarget() : NULL;
        MTGAbility * a = NEW AModifyHand(observer, id, card, t, splitHandMod[1], who);
        return a;
    }

    //Extra for Bestow
    vector<string> splitAuraIncreaseReduce = parseBetween(s, "modbenchant(", ")", true);
    if(splitAuraIncreaseReduce.size())
    {
        if(splitAuraIncreaseReduce[1].size())
        {
            Damageable * t = spell ? spell->getNextDamageableTarget() : NULL;
            vector<string> ccParameters = split( splitAuraIncreaseReduce[1], ':');
            int amount = 0;
            WParsedInt* parser = NEW WParsedInt(ccParameters[1], card);
            if(parser){
                amount = parser->intValue;
                SAFE_DELETE(parser);
            }
            int color = Constants::GetColorStringIndex(ccParameters[0]);
            if(ccParameters[0] == "colorless")
                color = 0;
            if(ccParameters[0].size() && ccParameters[1].size())
            {
                MTGAbility * a = NEW AAuraIncreaseReduce(observer, id, card, t, amount, color, who);
                return a;
            }
        }
    }

    //set hand size
    vector<string> splitSetHand = parseBetween(s, "sethand:", " ", false);
    if (splitSetHand.size())
    {
        int hand = 0;
        WParsedInt* parser = NEW WParsedInt(splitSetHand[1], card);
        if(parser){
            hand = parser->intValue;
            SAFE_DELETE(parser);
        }
        Damageable * t = spell ? spell->getNextDamageableTarget() : NULL;
        MTGAbility * a = NEW AASetHand(observer, id, card, t, hand, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //set life total
    vector<string> splitLifeset = parseBetween(s, "lifeset:", " ", false);
    if (splitLifeset.size())
    {
        WParsedInt * life = NEW WParsedInt(splitLifeset[1], spell, card);
        Damageable * t = spell ? spell->getNextDamageableTarget() : NULL;
        MTGAbility * a = NEW AALifeSet(observer, id, card, t, life, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //gain/lose life
    vector<string> splitLife = parseBetween(s, "life:", " ", false);
    if (splitLife.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AALifer(observer, id, card, t, splitLife[1], false, NULL, who);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AALifer*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //siphon life - gain life lost this way
    vector<string> splitSiphonLife = parseBetween(s, "lifeleech:", " ", false);
    if (splitSiphonLife.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AALifer(observer, id, card, t, splitSiphonLife[1], true, NULL, who);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AALifer*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    // Win the game
    found = s.find("wingame");
    if (found != string::npos)
    {
        Damageable * d = spell ?  spell->getNextDamageableTarget() : NULL;
        MTGAbility * a = NEW AAWinGame(observer, id, card, d, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //Draw
    vector<string> splitDraw = parseBetween(s, "draw:", " ", false);
    if (splitDraw.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AADrawer(observer, id, card, t, NULL,splitDraw[1], who,s.find("noreplace") != string::npos);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AADrawer*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Deplete
    vector<string> splitDeplete = parseBetween(s, "deplete:", " ", false);
    if (splitDeplete.size())
    {
        bool namerepeat = false;
        bool colorrepeat = false;
        if (splitDeplete[0].find("color") != string::npos)
            colorrepeat = true;
        if (splitDeplete[0].find("name") != string::npos)
            namerepeat = true;
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AADepleter(observer, id, card, t , splitDeplete[1], NULL, who, false, colorrepeat, namerepeat);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AADepleter*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Ingest
    vector<string> splitIngest = parseBetween(s, "ingest:", " ", false);
    if (splitIngest.size())
    {
        bool namerepeat = false;
        bool colorrepeat = false;
        if (splitIngest[0].find("coloringest") != string::npos)
            colorrepeat = true;
        if (splitIngest[0].find("nameingest") != string::npos)
            namerepeat = true;
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AADepleter(observer, id, card, t , splitIngest[1], NULL, who, true, colorrepeat, namerepeat);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AADepleter*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Cascade
    vector<string> splitCascade = parseBetween(s, "cascade:", " ", false);
    if (splitCascade.size())
    {
        MTGAbility * a = NEW AACascade(observer, id, card, target, splitCascade[1], NULL);
        a->oneShot = 1;
        return a;
    }

    //modify turns
    vector<string> splitModTurn = parseBetween(s, "turns:", " ", false);
    if (splitModTurn.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAModTurn(observer, id, card, t , splitModTurn[1], NULL, who);
        a->oneShot = 1;
        return a;
    }

    //Shuffle
    found = s.find("shuffle");
    if (found != string::npos)
    {
        Targetable * t = spell? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAShuffle(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAShuffle*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Serum Powder
    found = s.find("serumpowder");
    if (found != string::npos)
    {
        Targetable * t = spell? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAMulligan(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //Remove Mana from ManaPool
    vector<string> splitRemove = parseBetween(s, "removemana(", ")");
    if (splitRemove.size())
    {
        Targetable * t = spell? spell->getNextTarget() : NULL;
        bool forceclean = false;
        if(s.find("forceclean")!=string::npos)
            forceclean = true;
        MTGAbility *a = NEW AARemoveMana(observer, id, card, t, splitRemove[1], who, forceclean);
        a->oneShot = 1;
        return a;
    }

    //Lose subtypes of a given type
    vector<string> splitLoseTypes = parseBetween(s, "losesubtypesof(", ")");
    if (splitLoseTypes.size())
    {
        int parentType = MTGAllCards::findType(splitLoseTypes[1]);
        return NEW ALoseSubtypes(observer, id, card, target, parentType, false);
    }

    //Lose a specific type (e.g. "Conversion").
    vector<string> splitLoseSpecType = parseBetween(s, "losesatype(", ")");
    if (splitLoseSpecType.size())
    {
        int typeToLose = MTGAllCards::findType(splitLoseSpecType[1]);
        return NEW ALoseSubtypes(observer, id, card, target, typeToLose, true);
    }

    //Cast/Play Restrictions
    for (size_t i = 0; i < kMaxCastKeywordsCount; ++i)
    {
        vector<string> splitCast = parseBetween(s, kMaxCastKeywords[i], ")");
        if (splitCast.size())
        {
            TargetChooserFactory tcf(observer);
            TargetChooser * castTargets = tcf.createTargetChooser(splitCast[1], card);

            vector<string> splitValue = parseBetween(splitCast[2], "", " ", false);
            if (!splitValue.size())
            {
                DebugTrace ("MTGABILITY: Error parsing Cast/Play Restriction" << s);
                return NULL;
            }

            string valueStr = splitValue[1];
            bool modifyExisting = (valueStr.find("+") != string::npos || valueStr.find("-") != string::npos);

            WParsedInt * value = NEW WParsedInt(valueStr, spell, card);
            Targetable * t = spell? spell->getNextTarget() : NULL;
            if (((card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY)) && !forceForever && !untilYourNextEndTurn && !untilYourNextTurn) || forceUEOT)
            {
                return NEW AInstantCastRestrictionUEOT(observer, id, card, t, castTargets, value, modifyExisting, kMaxCastZones[i], who);
            }
            return NEW ACastRestriction(observer, id, card, t, castTargets, value, modifyExisting, kMaxCastZones[i], who);
        }
    }

    //Discard
    vector<string> splitDiscard = parseBetween(s, "discard:", " ", false);
    if (splitDiscard.size())
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AARandomDiscarder(observer, id, card, t, splitDiscard[1], NULL, who);
        a->oneShot = 1;
        return a;
    }

    //rampage
    vector<string> splitRampage = parseBetween(s, "rampage(", ")");
    if (splitRampage.size())
    {
        replace(splitRampage[1].begin(), splitRampage[1].end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using rampage keyword inside transforms)
        vector<string> rampageParameters = split(splitRampage[1], ',');
        int power, toughness;
        if (!parsePowerToughness(rampageParameters[0], &power, &toughness))
        {
            DebugTrace("MTGAbility Parse error in rampage" << s);
            return NULL;
        }
        int MaxOpponent = 0;
        WParsedInt* parser = NEW WParsedInt(rampageParameters[1], card);
        if(parser){
            MaxOpponent = parser->intValue;
            SAFE_DELETE(parser);
        }
        return NEW ARampageAbility(observer, id, card, power, toughness, MaxOpponent);
    }

    //evole
    if (s.find("evolve") != string::npos)
    {
        return NEW AEvolveAbility(observer, id, card);
    }

    //produce additional mana when tapped for mana
    if (s.find("produceextra:") != string::npos)
    {
        return NEW AProduceMana(observer, id, card,s.substr(13));
    }

    //produce additional mana when a mana is engaged
    if (s.find("producecolor:") != string::npos)
    {
        return NEW AEngagedManaAbility(observer, id, card,s.substr(13));
    }

    //reducelife to specific value
    if (s.find("reduceto:") != string::npos)
    {
        return NEW AReduceToAbility(observer, id, card,s.substr(9));
    }

    //attack cost
    if (s.find("attackcost:") != string::npos)
    {
        return NEW AAttackSetCost(observer, id, card, s.substr(11));
    }

    //attack cost + planeswalker
    if (s.find("attackpwcost:") != string::npos)
    {
        return NEW AAttackSetCost(observer, id, card, s.substr(13),true);
    }

    //block cost
    if (s.find("blockcost:") != string::npos)
    {
        return NEW ABlockSetCost(observer, id, card, s.substr(10));
    }

    //flanking
    if (s.find("flanker") != string::npos)
    {
        return NEW AFlankerAbility(observer, id, card);
    }

    //spirit link
    //combat damage spirit link
    if (s.find("spiritlink") != string::npos)
    {
        bool combatOnly = false;
        if(s.find("combatspiritlink") != string::npos)
        {
            combatOnly = true;
        }
        return NEW ASpiritLinkAbility(observer, id, card, combatOnly);
    }

    //bushido
    vector<string> splitBushido = parseBetween(s, "bushido(", ")");
    if (splitBushido.size())
    {
        string power, toughness;
        vector<string>splitPT = split(splitBushido[1],'/');
        if(!splitPT.size())
            return NULL;
        return NEW ABushidoAbility(observer, id, card,splitBushido[1],splitPT[0]);
    }
    vector<string> splitPhaseAlter = parseBetween(s, "phasealter(", ")");
    if (splitPhaseAlter.size())
    {
        string power, toughness;
        replace(splitPhaseAlter[1].begin(), splitPhaseAlter[1].end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using phasealter keyword inside transforms)
        vector<string>splitPhaseAlter2 = split(splitPhaseAlter[1],',');
        if(splitPhaseAlter2.size() < 3)
            return NULL;
        string after = "";
        if(splitPhaseAlter2.size() > 3)
        {
            vector<string> splitPhaseAlterAfter = parseBetween(splitPhaseAlter2[3], "after<", ">");
            if(splitPhaseAlterAfter.size())
                after = splitPhaseAlterAfter[1];
        }
        MTGAbility * a1 = NEW APhaseAlter(observer, id, card, target,splitPhaseAlter2[0].find("add") != string::npos, splitPhaseAlter2[1],splitPhaseAlter2[2], s.find("nextphase") != string::npos,after);
        a1->canBeInterrupted = false;
        return NEW GenericAbilityMod(observer, 1, card,spell?spell->getNextDamageableTarget():(Damageable *) target, a1);
    }

    //loseAbilities
    if (s.find("loseabilities") != string::npos)
    {
        return NEW ALoseAbilities(observer, id, card, target);
    }

    //counter
    vector<string> splitCounter = parseBetween(s, "counter(", ")");
    if (splitCounter.size())
    {
        string counterString = splitCounter[1];
        Counter * counter = parseCounter(counterString, target, spell);
        if (!counter)
        {
            DebugTrace("MTGAbility: can't parse counter:" << s);
            return NULL;
        }
        bool noevent = (s.find("notrg") != string::npos)?true:false; // Added a way to don't trigger @counter effect.
        MTGAbility * a =
            NEW AACounter(observer, id, card, target,counterString, counter->name.c_str(), counter->power, counter->toughness, counter->nb, counter->maxNb, noevent);
        delete (counter);
        a->oneShot = 1;
        return a;
    }

    //bestow
    found = s.find("bstw");
    if (found != string::npos)
    {
        MTGAbility * a = NEW ABestow(observer, id, card, target);
        a->oneShot = 1;
        return a;

    }

    //no counters on target of optional type
    vector<string> splitCounterShroud = parseBetween(s, "countershroud(", ")");
    if (splitCounterShroud.size())
    {
        string counterShroudString = splitCounterShroud[1];
        Counter * counter = NULL;
        if(splitCounterShroud[1] == "any")
        {
            counter = NULL;
        }
        else
        {
            counter = parseCounter(counterShroudString, target, spell);
            if (!counter)
            {
                DebugTrace("MTGAbility: can't parse counter:" << s);
                return NULL;
            }
        }
        TargetChooser * csTc = NULL;
        if(splitCounterShroud[2].size() > 1)
        {
            TargetChooserFactory af(card->getObserver());
            csTc = af.createTargetChooser(splitCounterShroud[2],card);
        }
        MTGAbility * a = NEW ACounterShroud(observer, id, card, target,csTc,counter);
        return a;
    }
    //use counters to track by counters to track an efect by counter name.
    vector<string> splitCounterTracking = parseBetween(s, "countertrack(", ")");
    if (splitCounterTracking.size())
    {
        string splitCounterTrack = splitCounterTracking[1];
        return NEW ACounterTracker(observer, id, card, target,splitCounterTrack);
    }
    //duplicate counters
    vector<string> splitDuplicateCounters = parseBetween(s, "duplicatecounters(", ")");
    if (splitDuplicateCounters.size())
    {
        MTGAbility * a = NEW AADuplicateCounters(observer, id, card, target, NULL);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        string counterString = splitDuplicateCounters[1];
        if(counterString.find("all") != string::npos)
            ((AADuplicateCounters*)a)->allcounters = true;
        if(counterString.find("single") != string::npos)
            ((AADuplicateCounters*)a)->single = true;
        return a;
    }
    //remove single counter of any type
    vector<string> splitRemoveSpecificCounters = parseBetween(s, "removesinglecountertype(", ")");
    if (splitRemoveSpecificCounters.size())
    {
        int nb = 0;
        bool allcounters = false;
        string counterString = splitRemoveSpecificCounters[1];
        if(counterString.find("all") != string::npos){
            allcounters = true;
            size_t pos = counterString.find(",all");
            if(pos != string::npos)
                counterString.replace(pos, 4, "");
            pos = counterString.find("all,");
            if(pos != string::npos)
                counterString.replace(pos, 4, "");
        }
        WParsedInt* parser = NEW WParsedInt(counterString, card);
        if(parser){
            nb = parser->intValue;
            SAFE_DELETE(parser);
        }
        MTGAbility * a = NEW AARemoveSingleCounter(observer, id, card, target, NULL, nb);
        ((AARemoveSingleCounter*)a)->allcounters = allcounters;
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    //removes all counters of the specifified type.
    vector<string> splitRemoveCounter = parseBetween(s, "removeallcounters(", ")");
    if (splitRemoveCounter.size())
    {
        string counterString = splitRemoveCounter[1];
        if(counterString.find("all") != string::npos)
        {
            MTGAbility * a = NEW AARemoveAllCounter(observer, id, card, target, "All", 0, 0, 1, true);
            a->oneShot = 1;
            return a;
        }

        Counter * counter = parseCounter(counterString, target, spell);
        if (!counter)
        {
            DebugTrace("MTGAbility: can't parse counter:" << s);
            return NULL;
        }

        MTGAbility * a =
            NEW AARemoveAllCounter(observer, id, card, target, counter->name.c_str(), counter->power, counter->toughness, counter->nb,false);
        delete (counter);
        a->oneShot = 1;
        return a;
    }
    
    //Becomes... (animate artifact...: becomes(Creature, manacost/manacost)
    vector<string> splitBecomes = parseBetween(s, "becomes(", ")");
    if (splitBecomes.size())
    {
        replace(splitBecomes[1].begin(), splitBecomes[1].end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using becomes keyword inside transforms)
        vector<string> becomesParameters = split(splitBecomes[1], ',');
        string stypes = becomesParameters[0];
        string newPower = "";
        string newToughness = "";
        bool ptFound = false;
        if(becomesParameters.size() >1)
        {
            vector<string> pt = split(becomesParameters[1], '/');
            if(pt.size() > 1)
            {
                newPower = pt[0];
                newToughness = pt[1];
                ptFound = true;
            }
        }
        string sabilities = "";
        unsigned int becomesSize = ptFound?2:1;
        if(becomesParameters.size() > becomesSize)
        {
            for(unsigned int i = becomesSize;i < becomesParameters.size();i++)
            { 
                sabilities.append(becomesParameters[i].c_str());
                if(i+1 < becomesParameters.size())
                    sabilities.append(",");
            }
        }
        if (oneShot || forceUEOT || forceForever)
            return NEW ATransformerInstant(observer, id, card, target, stypes, sabilities,newPower,ptFound,newToughness,ptFound,vector<string>(),false,forceForever,untilYourNextTurn,untilYourNextEndTurn);

        return  NEW ATransformer(observer, id, card, target, stypes, sabilities,newPower,ptFound,newToughness,ptFound,vector<string>(),false,forceForever,untilYourNextTurn,untilYourNextEndTurn);
    }

    //Remake... (animate artifact...: Remake(Creature: manacost/manacost) - alternative
    vector<string> splitRemake = parseBetween(s, "remake(", ")");
    if (splitRemake.size())
    {
        vector<string> RemakeParameters = split(splitRemake[1], ':');
        string stypes = RemakeParameters[0];
        string newPower = "";
        string newToughness = "";
        bool ptFound = false;
        if(RemakeParameters.size() >1)
        {
            vector<string> pt = split(RemakeParameters[1], '/');
            if(pt.size() > 1)
            {
                newPower = pt[0];
                newToughness = pt[1];
                ptFound = true;
            }
        }
        string sabilities = "";
        unsigned int RemakeSize = ptFound?2:1;
        if(RemakeParameters.size() > RemakeSize)
        {
            for(unsigned int i = RemakeSize;i < RemakeParameters.size();i++)
            { 
                sabilities.append(RemakeParameters[i].c_str());
                if(i+1 < RemakeParameters.size())
                    sabilities.append(",");
            }
        }
        if (oneShot || forceUEOT || forceForever)
            return NEW ATransformerInstant(observer, id, card, target, stypes, sabilities,newPower,ptFound,newToughness,ptFound,vector<string>(),false,forceForever,untilYourNextTurn,untilYourNextEndTurn);

        return  NEW ATransformer(observer, id, card, target, stypes, sabilities,newPower,ptFound,newToughness,ptFound,vector<string>(),false,forceForever,untilYourNextTurn,untilYourNextEndTurn);
    }

    //bloodthirst
    vector<string> splitBloodthirst = parseBetween(s, "bloodthirst:", " ", false);
    if (splitBloodthirst.size())
    {
        int nb = 0;
        WParsedInt* parser = NEW WParsedInt(splitBloodthirst[1], card);
        if(parser){
            nb = parser->intValue;
            SAFE_DELETE(parser);
        }
        return NEW ABloodThirst(observer, id, card, target, nb);
    }

    //Vanishing
    vector<string> splitVanishing = parseBetween(s, "vanishing:", " ", false);
    if (splitVanishing.size())
    {
        int nb = 0;
        WParsedInt* parser = NEW WParsedInt(splitVanishing[1], card);
        if(parser){
            nb = parser->intValue;
            SAFE_DELETE(parser);
        }
        return NEW AVanishing(observer, id, card, NULL, restrictions, nb, "time");
    }

    //Fading
    vector<string> splitFading = parseBetween(s, "fading:", " ", false);
    if (splitFading.size())
    {
        int nb = 0;
        WParsedInt* parser = NEW WParsedInt(splitFading[1], card);
        if(parser){
            nb = parser->intValue;
            SAFE_DELETE(parser);
        }
        return NEW AVanishing(observer, id, card, NULL, restrictions, nb, "fade");
    }

    //Alter cost
    if (s.find("altercost(") != string::npos)
        return getManaReduxAbility(s.substr(s.find("altercost(") + 10), id, spell, card, target);

    //transforms (e.g. hivestone,living enchantment)
    found = s.find("transforms((");
    if (found != string::npos)
    {
        string extraTransforms = "";
        string transformsParamsString = "";
        transformsParamsString.append(storedString);//the string between found and real end is removed at start.
        
        found = transformsParamsString.find("transforms(("); // Try to handle transforms ability inside transforms keyword.
        if (found != string::npos && extraTransforms.empty())
        {
            size_t real_end = transformsParamsString.find("))", found);
            size_t stypesStartIndex = found + 12;
            extraTransforms.append(transformsParamsString.substr(stypesStartIndex, real_end - stypesStartIndex).c_str());
            transformsParamsString.erase(stypesStartIndex, real_end - stypesStartIndex);
        }
        vector<string> effectParameters = split( transformsParamsString, ',');
        string stypes = effectParameters[0];

        string sabilities = transformsParamsString.substr(stypes.length());
        bool newpowerfound = false;
        string newpower = "";
        bool newtoughnessfound = false;
        string newtoughness = "";
        vector <string> abilities = split(sabilities, ',');
        bool newAbilityFound = false;
        vector<string> newAbilitiesList;
        storedString.erase();
        storedString.append(extraTransforms);
        extraTransforms.erase();
        for (unsigned int i = 0 ; i < abilities.size() ; i++)
        {
            if(abilities[i].empty())
                abilities.erase(abilities.begin()+i);
        }            
        for(unsigned int j = 0;j < abilities.size();j++)
        {
            vector<string> splitPower = parseBetween(abilities[j], "setpower=", ",", false);
            if(splitPower.size())
            {
                newpowerfound = true;
                newpower = splitPower[1];
            }
            vector<string> splitToughness = parseBetween(abilities[j], "settoughness=", ",", false);
            if(splitToughness.size())
            {
                newtoughnessfound = true;
                newtoughness = splitToughness[1];
            }
            if(abilities[j].find("newability[") != string::npos)
            {
                size_t NewSkill = abilities[j].find("newability[");
                size_t NewSkillEnd = abilities[j].find_last_of("]");
                string newAbilities = abilities[j].substr(NewSkill + 11,NewSkillEnd - NewSkill - 11);
                size_t pos = newAbilities.find("transforms(())"); // Try to handle transforms ability inside transforms keyword.
                if(pos != string::npos)
                    newAbilities.replace(pos, 14, "transforms((" + storedString + "))");
                newAbilitiesList.push_back(newAbilities);
                newAbilityFound = true;
            }
        }
        size_t pos = sabilities.find("transforms(())"); // Try to handle transforms ability inside transforms keyword.
        if(pos != string::npos)
            sabilities.replace(pos, 14, "transforms((" + storedString + "))");

        if (oneShot || forceUEOT || forceForever)
            return NEW ATransformerInstant(observer, id, card, target, stypes, sabilities,newpower,newpowerfound,newtoughness,newtoughnessfound,newAbilitiesList,newAbilityFound,forceForever,untilYourNextTurn,untilYourNextEndTurn,newName);
        
        return NEW ATransformer(observer, id, card, target, stypes, sabilities,newpower,newpowerfound,newtoughness,newtoughnessfound,newAbilitiesList,newAbilityFound,forceForever,untilYourNextTurn,untilYourNextEndTurn,newName);

    }
    
    //Reveal:x (activate aility) 
    vector<string> splitReveal = parseBetween(s, "reveal:", "revealend", false);
    if (splitReveal.size())
    {
        string backup = storedAbilityString;
        storedAbilityString = "";//we clear the string here for cards that contain more than 1 reveal.
        GenericRevealAbility * a = NEW GenericRevealAbility(observer, id, card, target, backup);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        a->named = newName;
        return a;
    }

    //scry:x (activate aility) 
    vector<string> splitScry = parseBetween(s, "scry:", "scryend", false);
    if (splitScry.size())
    {
        string backup = storedAbilityString;
        storedAbilityString = "";//we clear the string here for cards that contain more than 1 reveal.
        GenericScryAbility * a = NEW GenericScryAbility(observer, id, card, target, backup);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }

    //meld helper class
    vector<string> splitMeldFrom = parseBetween(s, "meldfrom(", ")", true);
    if (splitMeldFrom.size())
    {
        string splitMeldNames = "";
        if (splitMeldFrom[1].size())
        {
            splitMeldNames = splitMeldFrom[1];
            replace(splitMeldNames.begin(), splitMeldNames.end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using meldfrom keyword inside transforms)
        }
        MTGAbility * a = NEW AAMeldFrom(observer, id, card, target, splitMeldNames);
        a->oneShot = true;
        return a;
    }

    //meld
    vector<string> splitMeld = parseBetween(s, "meld(", ")", true);
    if (splitMeld.size())
    {
        string splitMeldName = "";
        if (splitMeld[1].size())
        {
            splitMeldName = splitMeld[1];
            replace(splitMeldName.begin(), splitMeldName.end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using meld keyword inside transforms)
        }
        MTGAbility * a = NEW AAMeld(observer, id, card, target, splitMeldName);
        a->oneShot = true;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAMeld*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //doubleside
    vector<string> splitSide = parseBetween(s, "doubleside(", ")", true);
    if (splitSide.size() && card->currentZone != card->controller()->game->battlefield) // It's not allowed to turn side on battlefield.
    {
        string splitSideName = "";
        if (splitSide[1].size())
        {
            splitSideName = splitSide[1];
            replace(splitSideName.begin(), splitSideName.end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using doubleside keyword inside transforms)
        }
        MTGAbility * a = NEW AATurnSide(observer, id, card, target, splitSideName);
        a->oneShot = true;
        return a;
    }

    //flip
    vector<string> splitFlipStat = parseBetween(s, "flip(", ")", true);
    if(splitFlipStat.size())
    {
        string flipStats = "";
        if(splitFlipStat[1].size())
        {
            /*vector<string>FlipStats = split(splitFlipStat[1],'%');*/
            flipStats = splitFlipStat[1];
            replace(flipStats.begin(), flipStats.end(), '^', ','); // To allow the usage of ^ instead of , char (e.g. using flip keyword inside transforms)
        }
        vector<string> splitType = parseBetween(s, "forcetype(", ")", true); // Added to flip instants and sorceries as permanents (es. Zendikar Rising Modal Double Faced cards).
        string forcetype = "";
        if(splitType.size() && splitType[1].size())
        {
            forcetype = splitType[1];
        }
        bool backfromcopy = (s.find("undocpy") != string::npos)?true:false; // Added to undo the copy effect at end of turn (es. Scion of the Ur-Dragon).
        bool transmode = card->getdoubleFaced() == "kamiflip"?true:false;
        MTGAbility * a = NEW AAFlip(observer, id, card, target, flipStats, transmode, false, forcetype, backfromcopy);
        //andability
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAFlip*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //changecost - alternate for altercost
    vector<string> splitChangeCost = parseBetween(s, "changecost(", ")", true);
    if(splitChangeCost.size())
    {
        if(splitChangeCost[1].size())
        {
            vector<string> ccParameters = split( splitChangeCost[1], ':');
            int amount = 0;
            WParsedInt * value = NEW WParsedInt(ccParameters[1], NULL, card);
            if(value){
                amount = value->getValue();
                SAFE_DELETE(value);
            }
            int color = Constants::GetColorStringIndex(ccParameters[0]);
            if(ccParameters[0] == "colorless")
                color = 0;
            if(ccParameters[0].size() && ccParameters[1].size())
            {
                MTGAbility * a = NEW AAlterCost(observer, id, card, target, amount, color);
                return a;
            }
        }
    }

    //Mana Producer
    found = s.find("add");
    if (found != string::npos)
    {
        bool doesntEmptyTilueot = s.find("doesntempty") != string::npos;
        ManaCost * output = ManaCost::parseManaCost(s.substr(found),NULL,card);
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AManaProducer(observer, id, card, t, output, NULL, who,s.substr(found),doesntEmptyTilueot);
        a->oneShot = 1;
        if(newName.size())
            ((AManaProducer*)a)->menutext = newName;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AManaProducer*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //another mana producer exempted for canproduce
    found = s.find("out");
    if (found != string::npos)
    {
        bool doesntEmptyTilueot = s.find("doesntempty") != string::npos;
        ManaCost * output = ManaCost::parseManaCost(s.substr(found),NULL,card);
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AManaProducer(observer, id, card, t, output, NULL, who,s.substr(found),doesntEmptyTilueot);
        a->oneShot = 1;
        if(newName.size())
            ((AManaProducer*)a)->menutext = newName;
        return a;
    }

    //Protection from...
    vector<string> splitProtection = parseBetween(s, "protection from(", ")");
    if (splitProtection.size())
    {
        TargetChooserFactory tcf(observer);
        TargetChooser * fromTc = tcf.createTargetChooser(splitProtection[1], card);
        if (!fromTc)
            return NULL;
        fromTc->setAllZones();
        if (!activated)
        {
            if (((card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY)) && !forceForever && !untilYourNextEndTurn && !untilYourNextTurn) || forceUEOT)
            {
                MTGAbility * aPF = NEW AProtectionFrom(observer, id, card, target, fromTc, splitProtection[1]);
                return NEW GenericInstantAbility(observer, 1, card, (Damageable *) target, aPF);
            }
            return NEW AProtectionFrom(observer, id, card, target, fromTc, splitProtection[1]);
        }
        return NULL; //TODO
    }

    //targetter can not target...
    vector<string> splitCantTarget = parseBetween(s, "cantbetargetof(", ")");
    if (splitCantTarget.size())
    {
        TargetChooserFactory tcf(observer);
        TargetChooser * fromTc = tcf.createTargetChooser(splitCantTarget[1], card);
        if (!fromTc)
            return NULL;
        fromTc->setAllZones();
        if (!activated)
        {
            if (((card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY)) && !forceForever && !untilYourNextEndTurn && !untilYourNextTurn) || forceUEOT)
            {
                return NULL; //TODO
            }
            return NEW ACantBeTargetFrom(observer, id, card, target, fromTc);
        }
        return NULL; //TODO
    }
    
    //Can't be blocked by...need cantdefendagainst(
    vector<string> splitCantBlock = parseBetween(s, "cantbeblockedby(", ")");
    if (splitCantBlock.size())
    {
        TargetChooserFactory tcf(observer);
        TargetChooser * fromTc = tcf.createTargetChooser(splitCantBlock[1], card);
        if (!fromTc)
            return NULL;
        //default target zone to opponentbattlefield here?
        if (!activated)
        {
            if (((card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY)) && !forceForever && !untilYourNextEndTurn && !untilYourNextTurn) || forceUEOT)
            {
                return NULL; //TODO
            }
            return NEW ACantBeBlockedBy(observer, id, card, target, fromTc);
        }
        return NULL; //TODO
    }

    //cant be the blocker of targetchooser.
    vector<string> splitCantBeBlock = parseBetween(s, "cantbeblockerof(", ")");
    if (splitCantBeBlock.size())
    {
        TargetChooserFactory tcf(observer);
        TargetChooser * fromTc = NULL;
        if (splitCantBeBlock[1].find("this") == string::npos)
        {
            fromTc = tcf.createTargetChooser(splitCantBeBlock[1], card);
        }

            if(fromTc)
                return NEW ACantBeBlockerOf(observer, id, card, target, fromTc, false);//of a targetchooser
            else
                return NEW ACantBeBlockerOf(observer, id, card, target, fromTc, true);//blocker of the card source.

        return NULL;
    }
    
    //Change Power/Toughness
    WParsedPT * wppt = NEW WParsedPT(s, spell, card);
    bool nonstatic = false;
    if (wppt->ok)
    {
        if(s.find("nonstatic") != string::npos)
            nonstatic = true;
        if (!activated)
        {
            if (((card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY)) && !forceForever && !untilYourNextEndTurn && !untilYourNextTurn) || forceUEOT)
            {
                return NEW PTInstant(observer, id, card, target, wppt,s,nonstatic);
            }
            else if(s.find("cdaactive") != string::npos)
            {
                MTGAbility * a = NEW APowerToughnessModifier(observer, id, card, target, wppt,s,true);
                a->forcedAlive = 1;
                //a->forceDestroy = -1;
                return a;
                //return NEW APowerToughnessModifier(observer, id, card, target, wppt,s,true);
            }
            else
                return NEW APowerToughnessModifier(observer, id, card, target, wppt,s,nonstatic);
        }
        return NEW PTInstant(observer, id, card, target, wppt,s,nonstatic);
    }
    else
    {
        delete wppt;
    }   

    //affinity based on targetchooser
    vector<string> splitNewAffinity = parseBetween(s, "affinity(", ")");
    if (splitNewAffinity.size())
    {
        string tcString = splitNewAffinity[1];
        string manaString = "";
        vector<string> splitNewAffinityMana = parseBetween(splitNewAffinity[2], "reduce(", ")");
        if(splitNewAffinityMana.size())
            manaString = splitNewAffinityMana[1];
        if(!manaString.size())
            return NULL;
        return NEW ANewAffinity(observer, id, card, tcString, manaString);
    }

    //proliferate, rule changes in War of the Spark set
    found = s.find("proliferate");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAProliferate(observer, id, card, target);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        bool noevent = (s.find("noproftrg") != string::npos)?true:false; // Added a way to don't trigger @proliferate effect.
        ((AAProliferate*)a)->allcounters = true;
        ((AAProliferate*)a)->notrigger = noevent;
        return a;
    }

    //proliferate all counters
    found = s.find("propagate");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAProliferate(observer, id, card, target);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        bool noevent = (s.find("noproftrg") != string::npos)?true:false; // Added a way to don't trigger @proliferated effect.
        ((AAProliferate*)a)->allcounters = true;
        ((AAProliferate*)a)->notrigger = noevent;
        return a;
    }

    //frozen, next untap this does not untap.
    found = s.find("frozen");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAFrozen(observer, id, card, target,false);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAFrozen*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //frozen, next untap this does not untap.
    found = s.find("freeze");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAFrozen(observer, id, card, target,true);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAFrozen*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //get a new target - retarget and newtarget makes the card refreshed - from exile to play (or from play to play)...
    if ((s.find("retarget") != string::npos) || s.find("newtarget") != string::npos)
    {
        MTGAbility * a = NEW AANewTarget(observer, id, card, target, NULL, (s.find("retarget") != string::npos), false, false, 0, (s.find("fromplay") != string::npos));
        a->oneShot = 1;
        if(s.find("untp") != string::npos)
            ((AANewTarget*)a)->untap = true;
        return a;
    }

    //get a new target for puresteel paladin...etc for equipments inplay only.. newhook & rehook supports stone hewer basic... the card is reequipped
    if ((s.find("rehook") != string::npos) || s.find("newhook") != string::npos)
    {
        MTGAbility * a = NEW AANewTarget(observer, id, card,target, NULL, false, true, (s.find("newhook") != string::npos));
        a->oneShot = 1;
        if(s.find("untp") != string::npos)
            ((AANewTarget*)a)->untap = true;
        return a;
    }

    //get a new target for mutations
    if ((s.find("mutateover") != string::npos) || s.find("mutateunder") != string::npos)
    {
        MTGAbility * a = NEW AANewTarget(observer, id, card, target, NULL, false, false, false, (s.find("mutateover") != string::npos)?1:2);
        a->oneShot = 1;
        return a;
    }

    //morph
    found = s.find("morph");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAMorph(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }
    //morph
    found = s.find("manafaceup");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAMorph(observer, id, card, target);
        a->oneShot = 1;
        ((AAMorph*)a)->face = true;
        return a;
    }

    //identify what a leveler creature will max out at.
    vector<string> splitMaxlevel = parseBetween(s, "maxlevel:", " ", false);
    if (splitMaxlevel.size())
    {
        int level = 0;
        WParsedInt* parser = NEW WParsedInt(splitMaxlevel[1], card);
        if(parser){
            level = parser->intValue;
            SAFE_DELETE(parser);
        }
        MTGAbility * a = NEW AAWhatsMax(observer, id, card, card, NULL, level);
        a->oneShot = 1;
        return a;
    }

    vector<string> splitCountObject = parseBetween(s, "count(", ")", false);
    if (splitCountObject.size())
    {
        MTGAbility * a = NEW AACountObject(observer, id, card, card, NULL, splitCountObject[1]);
        a->oneShot = 1;
        return a;
    }

    vector<string> splitCountObjectB = parseBetween(s, "countb(", ")", false);
    if (splitCountObjectB.size())
    {
        MTGAbility * a = NEW AACountObjectB(observer, id, card, card, NULL, splitCountObjectB[1]);
        a->oneShot = 1;
        return a;
    }

    //switch targest power with toughness
    found = s.find("swap");
    if (found != string::npos)
    {
        MTGAbility * a = NEW ASwapPTUEOT(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }
    //exchange life with target; if creature then toughness is life.
    found = s.find("exchangelife");
    if (found != string::npos)
    {
        Targetable * t = spell ? spell->getNextTarget() : NULL;
        MTGAbility * a = NEW AAExchangeLife(observer, id, card, t, NULL, who);
        a->oneShot = 1;
        return a;
    }

    //Regeneration
    found = s.find("regenerate");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AStandardRegenerate(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }
    
    //Gain/loose simple Ability
    for (int j = 0; j < Constants::NB_BASIC_ABILITIES; j++)
    {
        found = s.find(Constants::MTGBasicAbilities[j]);
        if (found == 0 || found == 1)
        {
            int modifier = 0;
            if(s.find("absorb") || s.find("flanking"))
                modifier += 1;
            else
                modifier = 1;

            if (found > 0 && s[found - 1] == '-')
                modifier = 0;
            else if(found > 0  && s[found - 1] == '-' && (s.find("absorb") || s.find("flanking")))
                modifier -= 1;

            if (!activated)
            {
                if (((card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY)) && !forceForever && !untilYourNextEndTurn && !untilYourNextTurn) || forceUEOT)
                {
                    return NEW AInstantBasicAbilityModifierUntilEOT(observer, id, card, target, j, modifier);
                }
                return NEW ABasicAbilityModifier(observer, id, card, target, j, modifier);
            }
            return NEW ABasicAbilityAuraModifierUntilEOT(observer, id, card, target, NULL, j, modifier);
        }
    }

    //Untapper (Ley Druid...)
    found = s.find("untap");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAUntapper(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AAUntapper*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //Tapper (icy manipulator)
    found = s.find("tap");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AATapper(observer, id, card, target, NULL, bool(s.find("tap(noevent)") != string::npos));
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((AATapper*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    //adds the rule to destroy children if parent dies
    found = s.find("connectrule");
    if(found != string::npos)
    {
        observer->addObserver(NEW ParentChildRule(observer, -1));
        observer->connectRule = true;
        return NULL;
    }
    //standard block
        found = s.find("block");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AABlock(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

    //create an association between cards.
    found = s.find("connect");
    if (found != string::npos)
    {
        MTGAbility * a = NEW AAConnect(observer, id, card, target);
        a->oneShot = 1;
        return a;
    }

    //steal target until source leaves battlefield
    found = s.find("steal");
    if (found != string::npos)
    {
        MTGAbility * a = NEW ASeizeWrapper(observer, id, card, target);
        a->oneShot = 1;
        if(storedAndAbility.size())
        {
            string stored = storedAndAbility;
            storedAndAbility.clear();
            ((ASeizeWrapper*)a)->andAbility = parseMagicLine(stored, id, spell, card);
        }
        return a;
    }

    DebugTrace(" no matching ability found. " << s);
    return NULL;
}

MTGAbility * AbilityFactory::parseUpkeepAbility(string s,MTGCardInstance * card,Spell * spell,int restrictions,int id)
{
    bool Cumulative = false;
    size_t cumulative = s.find("cumulativeupcost");
    if(cumulative != string::npos)
        Cumulative = true;
    size_t start = s.find("[");
    size_t end = s.find("]", start);
    string s1 = s.substr(start + 1, end - start - 1);
    size_t seperator = s1.find(";");
    int phase = MTG_PHASE_UPKEEP;
    int once = 0;
    if (seperator != string::npos)
    {
        for (int i = 0; i < NB_MTG_PHASES; i++)
        {
            if (s1.find("next") != string::npos)
                once = 1;

            string phaseStr = Constants::MTGPhaseCodeNames[i];
            if (s1.find(phaseStr) != string::npos)
            {
                phase = PhaseRing::phaseStrToInt(phaseStr);
                break;
            }

        }
        s1 = s1.substr(0, seperator);
    }
    ManaCost * cost = ManaCost::parseManaCost(s1);

    if (!cost)
    {
        DebugTrace("MTGABILITY: Parsing Error: " << s);
        return NULL;
    }

    string sAbility = s.substr(end + 1);
    MTGAbility * a = parseMagicLine(sAbility, id, spell, card);

    if (!a)
    {
        DebugTrace("MTGABILITY: Parsing Error: " << s);
        delete (cost);
        return NULL;
    }
    return  NEW AUpkeep(observer, id, card, a, cost, restrictions, phase, once,Cumulative);;
}

MTGAbility * AbilityFactory::parsePhaseActionAbility(string s,MTGCardInstance * card,Spell * spell,MTGCardInstance * target, int restrictions,int id)
{
    vector<string> splitActions = parseBetween(s, "[", "]");
    if (!splitActions.size())
    {
        DebugTrace("MTGABILITY:Parsing Error " << s);
        return NULL;
    }
    string s1 = splitActions[1];
    int phase = MTG_PHASE_UPKEEP;
    for (int i = 0; i < NB_MTG_PHASES; i++)
    {
        string phaseStr = Constants::MTGPhaseCodeNames[i];
        if (s1.find(phaseStr) != string::npos)
        {
            phase = PhaseRing::phaseStrToInt(phaseStr);
            break;
        }
    }

    bool opponentturn = (s1.find("my") == string::npos);
    bool myturn = (s1.find("opponent") == string::npos);
    bool sourceinPlay = (s1.find("sourceinplay") != string::npos);
    bool checkexile = (s1.find("checkex") != string::npos);
    bool next = (s1.find("next") == string::npos); //Why is this one the opposite of the two others? That's completely inconsistent
    bool once = (s1.find("once") != string::npos);

    MTGCardInstance * _target = NULL;
    if (spell)
        _target = spell->getNextCardTarget();
    if(!_target)
        _target = target;

    return NEW APhaseActionGeneric(observer, id, card, _target, trim(splitActions[2]), restrictions, phase, sourceinPlay, next, myturn, opponentturn, once, checkexile);
}

MTGAbility * AbilityFactory::parseChooseActionAbility(string s,MTGCardInstance * card,Spell *,MTGCardInstance * target, int, int id)
{
    vector<string> splitChooseAColor2 = parseBetween(s, "activatechooseacolor ", " activatechooseend");
    if (splitChooseAColor2.size())
    {
        string a1 = splitChooseAColor2[1];
        MTGAbility * a = NEW GenericChooseTypeColorName(observer, id, card, target,a1,true);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    //choose a type
    vector<string> splitChooseAType2 = parseBetween(s, "activatechooseatype ", " activatechooseend");
    if (splitChooseAType2.size())
    {
        string a1 = splitChooseAType2[1];
        MTGAbility * a = NEW GenericChooseTypeColorName(observer, id, card, target,a1,false,false,false,s.find("nonwall")!=string::npos);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    //choose a color
    vector<string> splitChooseAColor = parseBetween(s, "chooseacolor ", " chooseend");
    if (splitChooseAColor.size())
    {
        string a1 = splitChooseAColor[1];
        MTGAbility * a = NEW GenericChooseTypeColorName(observer, id, card, target,a1,true);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    //choose a type
    vector<string> splitChooseAType = parseBetween(s, "chooseatype ", " chooseend");
    if (splitChooseAType.size())
    {
        string a1 = splitChooseAType[1];
        MTGAbility * a = NEW GenericChooseTypeColorName(observer, id, card, target,a1,false,false,false,s.find("nonwall")!=string::npos);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    //choose a name
    vector<string> splitChooseAName = parseBetween(s, "chooseaname ", " chooseend");
    vector<string> splitChooseAOppName = parseBetween(s, "chooseanameopp ", " chooseend");
    if (splitChooseAName.size() || splitChooseAOppName.size())
    {
        bool oppName = (splitChooseAOppName.size() > 0);
        string a1 = oppName?splitChooseAOppName[1]:splitChooseAName[1];
        MTGAbility * a = NEW GenericChooseTypeColorName(observer, id, card, target,a1,false,!oppName,oppName,false,s.find("nonbasicland")!=string::npos,s.find("nonland")!=string::npos);
        a->oneShot = 1;
        a->canBeInterrupted = false;
        return a;
    }
    return NULL;
}

//Tells the AI if the ability should target itself or an ennemy
int AbilityFactory::abilityEfficiency(MTGAbility * a, Player * p, int mode, TargetChooser * tc,Targetable * target)
{
    if (!a)
        return BAKA_EFFECT_DONTKNOW;

    if (GenericTargetAbility * abi = dynamic_cast<GenericTargetAbility*>(a))
    {
        if (mode == MODE_PUTINTOPLAY)
            return BAKA_EFFECT_GOOD;
        return abilityEfficiency(abi->ability, p, mode, abi->getActionTc());
    }
    if (GenericActivatedAbility * abi = dynamic_cast<GenericActivatedAbility*>(a))
    {
        if (mode == MODE_PUTINTOPLAY)
            return BAKA_EFFECT_GOOD;
        return abilityEfficiency(abi->ability, p, mode, tc);
    }
    if (MultiAbility * abi = dynamic_cast<MultiAbility*>(a))
        return abilityEfficiency(abi->abilities[0], p, mode, tc);
    if (MayAbility * abi = dynamic_cast<MayAbility*>(a))
        return abilityEfficiency(abi->ability, p, mode, tc);
    if (ALord * abi = dynamic_cast<ALord *>(a))
    {
        int myCards = countCards(abi->getActionTc(), p);
        int theirCards = countCards(abi->getActionTc(), p->opponent());
        int efficiency = abilityEfficiency(abi->ability, p, mode, tc);
        if (efficiency == BAKA_EFFECT_GOOD)
        {
            myCards < theirCards? efficiency = BAKA_EFFECT_BAD : efficiency = BAKA_EFFECT_GOOD;
        }
        else if (efficiency == BAKA_EFFECT_BAD)
        {
            myCards >= theirCards? efficiency = BAKA_EFFECT_BAD : efficiency = BAKA_EFFECT_GOOD;
        }
        return efficiency;
        /*this method below leads to too many undesired effects, basically it doesn't work how the original coder thought it would.
        leaving it for reference to avoid it reaccuring during a refactor.
        if ( ((myCards <= theirCards) && efficiency == BAKA_EFFECT_GOOD) || ((myCards >= theirCards) && efficiency == BAKA_EFFECT_BAD)   )
        return efficiency;
        return -efficiency; */
    }
    if (AAsLongAs * abi = dynamic_cast<AAsLongAs *>(a))
        return abilityEfficiency(abi->ability, p, mode, tc);
    if (AForeach * abi = dynamic_cast<AForeach *>(a))
        return abilityEfficiency(abi->ability, p, mode, tc);
    if (ATeach * abi = dynamic_cast<ATeach *>(a))
        return abilityEfficiency(abi->ability, p, mode, tc);
    if (ATargetedAbilityCreator * atac = dynamic_cast<ATargetedAbilityCreator *>(a))
    {
        Player * targetedPlyr;
        switch(atac->who)
        {
        case TargetChooser::CONTROLLER:
            targetedPlyr = atac->source->controller();
            break;
        case TargetChooser::OPPONENT:
            targetedPlyr = atac->source->controller()->opponent();
            break;
        case TargetChooser::TARGET_CONTROLLER:
            if(dynamic_cast<MTGCardInstance*>(target))
            {
                targetedPlyr = ((MTGCardInstance*)atac->target)->controller();
                break;
            }
        case TargetChooser::TARGETED_PLAYER:
            {
                targetedPlyr = atac->source->playerTarget?atac->source->playerTarget:p;
                break;
            }
        default:
            targetedPlyr = atac->source->controller()->opponent();
            break;
        }
        int result = 0;
        if(targetedPlyr)
        {
            MTGCardInstance  * testDummy = NEW MTGCardInstance();
            testDummy->setObserver(targetedPlyr->getObserver());
            testDummy->owner = targetedPlyr;
            testDummy->storedSourceCard = atac->source;
            testDummy->lastController = targetedPlyr;
            vector<string>magictextlines = split(atac->sabilities,'_');
            if(magictextlines.size())
            {
                for(unsigned int i = 0; i < magictextlines.size(); i++)
                {
                    MTGAbility * ata = parseMagicLine(magictextlines[i],-1,NULL,testDummy);
                    if(ata)
                    {
                        result += abilityEfficiency(getCoreAbility(ata), targetedPlyr,mode);
                        SAFE_DELETE(ata);
                    }
                }
            }
            SAFE_DELETE(testDummy);
        }
        return result;
    }
    if (dynamic_cast<AAFizzler *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AADamagePrevent *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AACloner *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<ASwapPTUEOT *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AAUntapper *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AATapper *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AManaProducer *> (a))
        return BAKA_EFFECT_GOOD;
     if (dynamic_cast<AARemoveAllCounter *> (a))
         return BAKA_EFFECT_BAD;
     if (dynamic_cast<AAProliferate *> (a))
         return BAKA_EFFECT_GOOD;

    // Equipment that gets immediately attached. Todo: check the abilities associated with Equip, to make sure they're good (for now it seems to be the majority of the cases)?
    if (dynamic_cast<AEquip *> (a))
        return BAKA_EFFECT_GOOD;

    // For now, ACounterTracker is only used for Creatures that "belong" to one of our domains, need to target one of our own lands, so we return a "positive" value
    if (dynamic_cast<ACounterTracker *>(a))
        return BAKA_EFFECT_GOOD;

    if (AACounter * ac = dynamic_cast<AACounter *>(a))
    {
        bool negative_effect = ac->power < 0 || ac->toughness < 0;
        if ((ac->nb > 0 && negative_effect) || (ac->nb < 0 && !negative_effect))
            return BAKA_EFFECT_BAD;
        return BAKA_EFFECT_GOOD;
    }

    if (dynamic_cast<ATokenCreator *> (a))
        return BAKA_EFFECT_GOOD;

    if (AAMover * aam = dynamic_cast<AAMover *>(a))
    {
        MTGGameZone * z = aam->destinationZone(target);
        if ((tc && tc->targetsZone(p->game->library)) || (tc && tc->targetsZone(p->game->graveyard)) || (tc && tc->targetsZone(p->game->hand)))
        {
            if (z == p->game->hand || z == p->game->inPlay)
                return BAKA_EFFECT_GOOD;
        }
         return BAKA_EFFECT_BAD; //TODO
    }

    if (dynamic_cast<AACopier *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AABuryCard *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AADestroyCard *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AStandardRegenerate *> (a))
        return BAKA_EFFECT_GOOD;
    if (AALifer * abi = dynamic_cast<AALifer *>(a))
        return abi->getLife() > 0 ? BAKA_EFFECT_GOOD : BAKA_EFFECT_BAD;
    if (AAAlterPoison * abi = dynamic_cast<AAAlterPoison *>(a))
        return abi->poison > 0 ? BAKA_EFFECT_GOOD : BAKA_EFFECT_BAD;
    if (dynamic_cast<AADepleter *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AADrawer *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AARandomDiscarder *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<ARampageAbility *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<ABushidoAbility *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AACascade *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AACastCard *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AAFlip *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AAImprint *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AAHaunt *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AATrain *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<ABestow *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AExert *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<ALoseAbilities *> (a))
        return BAKA_EFFECT_BAD;
    if (dynamic_cast<AModularAbility *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<APaired *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AProduceMana *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AACloner *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AAModTurn *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<ATransformer *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AADamager *> (a))
        return BAKA_EFFECT_BAD;

    if (PTInstant * abi = dynamic_cast<PTInstant *>(a))
        return (abi->wppt->power.getValue() >= 0 && abi->wppt->toughness.getValue() >= 0) ? BAKA_EFFECT_GOOD : BAKA_EFFECT_BAD;
    if (APowerToughnessModifier * abi = dynamic_cast<APowerToughnessModifier *>(a))
        return (abi->wppt->power.getValue() >= 0 && abi->wppt->toughness.getValue() >= 0) ? BAKA_EFFECT_GOOD : BAKA_EFFECT_BAD;
    if (PTInstant * abi = dynamic_cast<PTInstant *>(a))
        return abilityEfficiency(abi->ability, p, mode, tc);

    if (dynamic_cast<ACantBeBlockedBy *> (a))
        return BAKA_EFFECT_GOOD;
    if (dynamic_cast<AProtectionFrom *> (a))
        return BAKA_EFFECT_GOOD;

    map<int, bool> badAbilities;
    badAbilities[(int)Constants::CANTATTACK] = true;
    badAbilities[(int)Constants::CANTBLOCK] = true;
    badAbilities[(int)Constants::CLOUD] = true;
    badAbilities[(int)Constants::DEFENDER] = true;
    badAbilities[(int)Constants::DOESNOTUNTAP] = true;
    badAbilities[(int)Constants::MUSTATTACK] = true;
    badAbilities[(int)Constants::CANTREGEN] = true;
    badAbilities[(int)Constants::NOACTIVATED] = true;
    badAbilities[(int)Constants::NOACTIVATEDTAP] = true;
    badAbilities[(int)Constants::NOMANA] = true;
    badAbilities[(int)Constants::ONLYMANA] = true;
    badAbilities[(int)Constants::EXILEDEATH] = true;
    badAbilities[(int)Constants::GAINEDEXILEDEATH] = true;
    badAbilities[(int)Constants::HANDDEATH] = true;
    badAbilities[(int)Constants::GAINEDHANDDEATH] = true;
    badAbilities[(int)Constants::INPLAYDEATH] = true;
    badAbilities[(int)Constants::COUNTERDEATH] = true;
    badAbilities[(int)Constants::INPLAYTAPDEATH] = true;
    badAbilities[(int)Constants::DOUBLEFACEDEATH] = true;
    badAbilities[(int)Constants::GAINEDDOUBLEFACEDEATH] = true;
    badAbilities[(int)Constants::WEAK] = true;
    badAbilities[(int)Constants::NOLIFEGAIN] = true;
    badAbilities[(int)Constants::NOLIFEGAINOPPONENT] = true;
    badAbilities[(int)Constants::MUSTBLOCK] = true;
    badAbilities[(int)Constants::FLYERSONLY] = true;
    badAbilities[(int)Constants::TREASON] = true;
    badAbilities[(int)Constants::SHACKLER] = true;

    if (AInstantBasicAbilityModifierUntilEOT * abi = dynamic_cast<AInstantBasicAbilityModifierUntilEOT *>(a))
    {
        int result = badAbilities[abi->ability] ? BAKA_EFFECT_BAD : BAKA_EFFECT_GOOD;
        return (abi->value > 0) ? result : -result;
    }
    if (ABasicAbilityModifier * abi = dynamic_cast<ABasicAbilityModifier *>(a))
    {
        int result = (badAbilities[abi->ability]) ? BAKA_EFFECT_BAD : BAKA_EFFECT_GOOD;
        return (abi->modifier > 0) ? result : -result;
    }
    if (ABasicAbilityAuraModifierUntilEOT * abi = dynamic_cast<ABasicAbilityAuraModifierUntilEOT *>(a))
        return abilityEfficiency(abi->ability, p, mode);
    if (dynamic_cast<AManaProducer*> (a))
        return BAKA_EFFECT_GOOD;
    return BAKA_EFFECT_DONTKNOW;
}

//Returns the "X" cost that was paid for a spell
int AbilityFactory::computeX(Spell * spell, MTGCardInstance * card)
{
    if (spell)
        return spell->computeX(card);
    if(card) return card->X;
    return 0;
}

int AbilityFactory::getAbilities(vector<MTGAbility *> * v, Spell * spell, MTGCardInstance * card, int id, MTGGameZone * dest)
{
    if (!card && spell)
        card = spell->source;
    if (!card)
        return 0;

    string magicText;
    if (dest)
    {
        card->graveEffects = false;
        card->exileEffects = false;
        card->commandZoneEffects = false;
        card->handEffects = false;
        for (int i = 0; i < 2; ++i)
        {
            MTGPlayerCards * zones = observer->players[i]->game;
            if (dest == zones->hand)
            {
                magicText = card->magicTexts["hand"];
                card->handEffects = true;
                break;
            }
            if (dest == zones->graveyard)
            {
                magicText = card->magicTexts["graveyard"];
                card->graveEffects = true;
                break;
            }
            if (dest == zones->stack)
            {
                magicText = card->magicTexts["stack"];
                break;
            }
            if (dest == zones->exile)
            {
                magicText = card->magicTexts["exile"];
                card->exileEffects = true;
                break;
            }
            if (dest == zones->commandzone)
            {
                magicText = card->magicTexts["commandzone"];
                card->commandZoneEffects = true;
                break;
            }
            if (dest == zones->library)
            {
                magicText = card->magicTexts["library"];
                break;
            }
            //Other zones needed ?
        }
    }
    else
    {
        if(card->previous && card->previous->morphed && !card->turningOver)
        {
            magicText = card->magicTexts["facedown"];
            card->power = 2;
            card->life = 2;
            card->toughness = 2;
            card->setColor(0,1);
            card->name = "Morph";
            card->types.clear();
            string cre = "Creature";
            card->setType(cre.c_str());
            for(size_t i = 0; i < card->basicAbilities.size(); i++) {
                if(i != Constants::GAINEDEXILEDEATH && i != Constants::GAINEDHANDDEATH && i != Constants::GAINEDDOUBLEFACEDEATH && 
                    i != Constants::DUNGEONCOMPLETED && i != Constants::PERPETUALDEATHTOUCH && i != Constants::PERPETUALLIFELINK)
                    card->basicAbilities[i] = 0; // Try to keep the original special abilities on card morph.
            }
            card->getManaCost()->resetCosts();
        }
        else if(card && !card->morphed && card->turningOver)
        {
            card->power += card->origpower-2;
            card->life += card->origtoughness-2;
            card->toughness += card->origtoughness-2;
            card->setColor(0,1);
            card->name = card->model->data->name;
            card->types = card->model->data->types;
            card->colors = card->model->data->colors;
            card->basicAbilities |= card->model->data->basicAbilities;
            ManaCost * copyCost = card->model->data->getManaCost();
            card->getManaCost()->copy(copyCost);
            magicText = card->model->data->magicText;
            string faceupC= card->magicTexts["faceup"];
            magicText.append("\n");
            magicText.append(faceupC);
        }
        else if(card && card->hasType(Subtypes::TYPE_EQUIPMENT) && card->target)
        {
            magicText = card->model->data->magicText;
            string equipText = card->magicTexts["skill"];
            magicText.append("\n");
            magicText.append(equipText);
        }
        else
        {
            magicText = card->magicText;
        }
    }
    if (card->alias && magicText.size() == 0 && !dest)
    {
        MTGCard * c = MTGCollection()->getCardById(card->alias);
        if (!c)
            return 0;
        magicText = c->data->magicText;
    }
    string line;
    int size = magicText.size();
    if (size == 0)
        return 0;
    size_t found;
    int result = id;

    magicText = AutoLineMacro::Process(magicText);


    while (magicText.size())
    {
        found = magicText.find("\n");
        if (found != string::npos)
        {
            line = magicText.substr(0, found);
            magicText = magicText.substr(found + 1);
        }
        else
        {
            line = magicText;
            magicText = "";
        }
        MTGAbility * a = parseMagicLine(line, result, spell, card, false, false, dest);
        if (a)
        {
            v->push_back(a);
            result++;
        }
        else
        {
            DebugTrace("ABILITYFACTORY ERROR: Parser returned NULL " + magicText);
        }
    }
    return result;
}

//Some basic functionalities that can be added automatically in the text file
/*
 * Several objects are computed from the text string, and have a direct influence on what action we should take
 * (direct impact on the game such as draw a card immediately, or create a New GameObserver and add it to the Abilities,etc..)
 * These objects are:
 *   - trigger (if there is an "@" in the string, this is a triggered ability)
 *   - target (if there ie a "target(" in the string, then this is a TargetAbility)
 *   - doTap (a dirty way to know if tapping is included in the cost...
 */
int AbilityFactory::magicText(int id, Spell * spell, MTGCardInstance * card, int mode, TargetChooser * tc, MTGGameZone * dest)
{try{
    int dryMode = 0;
    if (!spell && !dest)
        dryMode = 1;

    vector<MTGAbility *> v;
    int result = getAbilities(&v, spell, card, id, dest);

    for (size_t i = 0; i < v.size(); ++i)
    {
        MTGAbility * a = v[i];
        if (!a)
        {
            DebugTrace("ABILITYFACTORY ERROR: Parser returned NULL");
            continue;
        }

        if (dryMode)
        {
            result = abilityEfficiency(a, card->controller(), mode, tc);
            for (size_t i = 0; i < v.size(); ++i)
                SAFE_DELETE(v[i]);
            return result;
        }

        if (!a->oneShot)
        {
            // Anything involving Mana Producing abilities cannot be interrupted
            MTGAbility * core = getCoreAbility(a);
            if (dynamic_cast<AManaProducer*> (core))
                a->canBeInterrupted = false;
        }

        bool moreThanOneTarget = spell && spell->tc && spell->tc->getNbTargets() > 1;

        if(moreThanOneTarget)
            a->target = spell->getNextTarget();

        if(a->target && moreThanOneTarget) 
        {
            MayAbility * aMay = dynamic_cast<MayAbility*>(a);
            while(a->target)
            {
                if(a->oneShot)
                {
                    a->resolve();
                }
                else
                {
                    if(!aMay || (aMay && a->target == spell->tc->getNextTarget(0)))
                    {
                        MTGAbility * mClone = a->clone();
                        mClone->addToGame();
                    }
                }
                a->target = spell->getNextTarget(a->target);
            }
            SAFE_DELETE(a);
        }
        else
        {
            if (a->oneShot)
            {
                a->resolve();
                delete (a);
            }
            else
            {
                a->addToGame();
                MayAbility * dontAdd = dynamic_cast<MayAbility*>(a);
                if(!dontAdd)
                {
                if (a->source)
                    a->source->cardsAbilities.push_back(a);
                else if(spell && spell->source)
                    spell->source->cardsAbilities.push_back(a);
                }
                //keep track of abilities being added to the game on each card it belongs to, this ignores p/t bonuses given
                //from other cards, or ability bonuses, making it generally easier to strip a card of it's abilities.
            }
        }
    }

    return result;
    }
    catch(exception) {
     DebugTrace("MAGIC TEST ERROR: Parser returned NULL");
    }
    return 0;
}

void AbilityFactory::addAbilities(int _id, Spell * spell)
{
    MTGCardInstance * card = spell->source;

    if (spell->getNbTargets() == 1)
    {
        card->target = spell->getNextCardTarget();
        if (card->target && (!spell->tc->canTarget(card->target) || card->target->isPhased))
        {
            MTGPlayerCards * zones = card->controller()->game;
            zones->putInZone(card, spell->from, card->owner->game->graveyard);
            return; //fizzle
        }
        card->playerTarget = spell->getNextPlayerTarget();
    } 
    if(!card->playerTarget && card->previous && card->previous->playerTarget)
        card->playerTarget = card->previous->playerTarget;//instants seem to forget as they travel from zone to zone.
    _id = magicText(_id, spell);

    MTGPlayerCards * zones = card->controller()->game;

    int id = card->alias;
    switch (id)
    {
    case 1092: //Aladdin's lamp
    {
        AAladdinsLamp * ability = NEW AAladdinsLamp(observer, _id, card);
        observer->addObserver(ability);
        break;
    }
    case 1095: //Armageddon clock
    {
        AArmageddonClock * ability = NEW AArmageddonClock(observer, _id, card);
        observer->addObserver(ability);
        break;
    }

    case 1191: //Blue Elemental Blast
    {
        if (card->target)
        {
            card->target->controller()->game->putInGraveyard(card->target);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            observer->mLayers->stackLayer()->Fizzle(starget);
        }
        break;
    }
    case 1282: //Chaoslace
    {
        if (card->target)
        {
            card->target->setColor(Constants::MTG_COLOR_RED, 1);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            starget->source->setColor(Constants::MTG_COLOR_RED, 1);
        }
        break;
    }
    case 1335: //Circle of protection : black
    {
        observer->addObserver(NEW ACircleOfProtection(observer, _id, card, Constants::MTG_COLOR_BLACK));
        break;
    }
    case 1336: //Circle of protection : blue
    {
        observer->addObserver(NEW ACircleOfProtection(observer, _id, card, Constants::MTG_COLOR_BLUE));
        break;
    }
    case 1337: //Circle of protection : green
    {
        observer->addObserver(NEW ACircleOfProtection(observer, _id, card, Constants::MTG_COLOR_GREEN));
        break;
    }
    case 1338: //Circle of protection : red
    {
        observer->addObserver(NEW ACircleOfProtection(observer, _id, card, Constants::MTG_COLOR_RED));
        break;
    }
    case 1339: //Circle of protection : white
    {
        observer->addObserver(NEW ACircleOfProtection(observer, _id, card, Constants::MTG_COLOR_WHITE));
        break;
    }
    case 1102: //Conservator
    {
        observer->addObserver(NEW AConservator(observer, _id, card));
        break;
    }

    case 1103: //Crystal Rod
    {
        
        std::vector<int16_t> cost;
        cost.push_back(Constants::MTG_COLOR_ARTIFACT);
        cost.push_back(1);
        ASpellCastLife* ability = NEW ASpellCastLife(observer, _id, card, Constants::MTG_COLOR_BLUE, NEW ManaCost(cost, 1), 1);
        observer->addObserver(ability);
        break;
    }
    case 1152: //Deathlace
    {
        if (card->target)
        {
            card->target->setColor(Constants::MTG_COLOR_BLACK, 1);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            starget->source->setColor(Constants::MTG_COLOR_BLACK, 1);
        }
        break;
    }

    case 1291: //Fireball
    {
        int x = computeX(spell, card);
        observer->addObserver(NEW AFireball(observer, _id, card, spell, x));
        break;
    }
    case 1113: //Iron Star
    {
        
        std::vector<int16_t> cost;
        cost.push_back(Constants::MTG_COLOR_ARTIFACT);
        cost.push_back(1);
        ASpellCastLife* ability = NEW ASpellCastLife(observer, _id, card, Constants::MTG_COLOR_RED, NEW ManaCost(cost, 1), 1);
        observer->addObserver(ability);
        break;
    }
    case 1351: // Island Sanctuary
    {
        observer->addObserver(NEW AIslandSanctuary(observer, _id, card));
        break;
    }
    case 1114: //Ivory cup
    {
        std::vector<int16_t> cost;
        cost.push_back(Constants::MTG_COLOR_ARTIFACT);
        cost.push_back(1);
        ASpellCastLife* ability = NEW ASpellCastLife(observer, _id, card, Constants::MTG_COLOR_WHITE, NEW ManaCost(cost, 1), 1);
        observer->addObserver(ability);
        break;
    }
    case 1117: //Jandors Ring
    {
        observer->addObserver(NEW AJandorsRing(observer, _id, card));
        break;
    }
    case 1257: //Lifelace
    {
        if (card->target)
        {
            card->target->setColor(Constants::MTG_COLOR_GREEN, 1);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            starget->source->setColor(Constants::MTG_COLOR_GREEN, 1);
        }
        break;
    }
    case 1124: //Mana Vault (the rest is softcoded!)
    {
        observer->addObserver(NEW ARegularLifeModifierAura(observer, _id + 2, card, card, MTG_PHASE_DRAW, -1, 1));
        break;
    }
    case 1215: //Power Leak
    {
        observer->addObserver(NEW APowerLeak(observer, _id, card, card->target));
        break;
    }
    case 1358: //Purelace
    {
        if (card->target)
        {
            card->target->setColor(Constants::MTG_COLOR_WHITE, 1);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            starget->source->setColor(Constants::MTG_COLOR_WHITE, 1);
        }
        break;
    }
    case 1312: //Red Elemental Blast
    {
        if (card->target)
        {
            card->target->controller()->game->putInGraveyard(card->target);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            observer->mLayers->stackLayer()->Fizzle(starget);
        }
        break;
    }

    case 1139: //The Rack
    {
        observer->addObserver(NEW ALifeZoneLink(observer, _id, card, MTG_PHASE_UPKEEP, -3));
        break;
    }

    case 1140: //Throne of Bone
    {
        std::vector<int16_t> cost;
        cost.push_back(Constants::MTG_COLOR_ARTIFACT);
        cost.push_back(1);
        ASpellCastLife* ability = NEW ASpellCastLife(observer, _id, card, Constants::MTG_COLOR_BLACK, NEW ManaCost(cost, 1), 1);
        observer->addObserver(ability);
        break;
    }

    case 1142: //Wooden Sphere
    {
        std::vector<int16_t> cost;
        cost.push_back(Constants::MTG_COLOR_ARTIFACT);
        cost.push_back(1);
        ASpellCastLife* ability = NEW ASpellCastLife(observer, _id, card, Constants::MTG_COLOR_GREEN, NEW ManaCost(cost, 1), 1);
        observer->addObserver(ability);
        break;
    }
    case 1143: //Animate Dead
    {
        AAnimateDead * a = NEW AAnimateDead(observer, _id, card, card->target);
        observer->addObserver(a);
        card->target = ((MTGCardInstance *) a->target);
        break;
    }
    case 1156: //Drain Life
    {
        Damageable * target = spell->getNextDamageableTarget();
        int x = spell->cost->getConvertedCost() - 2; //TODO Fix that !!! + X should be only black mana, that needs to be checked !
        observer->mLayers->stackLayer()->addDamage(card, target, x);
        if (target->life < x)
            x = target->life;
        observer->currentlyActing()->gainLife(x, card);
        break;
    }
    case 1159: //Erg Raiders
    {
        AErgRaiders* ability = NEW AErgRaiders(observer, _id, card);
        observer->addObserver(ability);
        break;
    }
    case 1202: //Hurkyl's Recall
    {
        Player * player = spell->getNextPlayerTarget();
        if (player)
        {
            for (int i = 0; i < 2; i++)
            {
                MTGInPlay * inplay = observer->players[i]->game->inPlay;
                for (int j = inplay->nb_cards - 1; j >= 0; j--)
                {
                    MTGCardInstance * card = inplay->cards[j];
                    if (card->owner == player && card->hasType(Subtypes::TYPE_ARTIFACT))
                    {
                        player->game->putInZone(card, inplay, player->game->hand);
                    }
                }
            }
        }
        break;
    }
    case 1209: //Mana Short
    {
        Player * player = spell->getNextPlayerTarget();
        if (player)
        {
            MTGInPlay * inplay = player->game->inPlay;
            for (int i = 0; i < inplay->nb_cards; i++)
            {
                MTGCardInstance * current = inplay->cards[i];
                if (current->hasType(Subtypes::TYPE_LAND))
                    current->tap();
            }
            player->getManaPool()->Empty();
        }
        break;
    }
    case 1167: //Mind Twist
    {
        int xCost = computeX(spell, card);
        for (int i = 0; i < xCost; i++)
        {
            observer->opponent()->game->discardRandom(observer->opponent()->game->hand, card);
        }
        break;
    }
    case 1176: //Sacrifice
    {
        ASacrifice * ability = NEW ASacrifice(observer, _id, card, card->target);
        observer->addObserver(ability);
        break;
    }
    case 1194: //Control Magic
    {
        observer->addObserver(NEW AControlStealAura(observer, _id, card, card->target));
        break;
    }
    case 1231: //Volcanic Eruption
    {
        int x = computeX(spell, card);
        int _x = x;
        MTGCardInstance * target = spell->getNextCardTarget();
        while (target && _x)
        {
            target->destroy();
            _x--;
            target = spell->getNextCardTarget(target);
        }
        x -= _x;
        for (int i = 0; i < 2; i++)
        {
            observer->mLayers->stackLayer()->addDamage(card, observer->players[i], x);
            for (int j = 0; j < observer->players[i]->game->inPlay->nb_cards; j++)
            {
                MTGCardInstance * current = observer->players[i]->game->inPlay->cards[j];
                if (current->isCreature())
                {
                    observer->mLayers->stackLayer()->addDamage(card, current, x);
                }
            }
        }
        break;
    }
    case 1288: //EarthBind
    {
        observer->addObserver(NEW AEarthbind(observer, _id, card, card->target));
        break;
    }
    case 1344: //Eye for an Eye
    {
        Damage * damage = spell->getNextDamageTarget();
        if (damage)
        {
            observer->mLayers->stackLayer()->addDamage(card, damage->source->controller(), damage->damage);
        }
        break;
    }
    case 1243: //Fastbond
    {
        observer->addObserver(NEW AFastbond(observer, _id, card));
        break;
    }
    case 1227: //Toughtlace
    {
        if (card->target)
        {
            card->target->setColor(Constants::MTG_COLOR_BLUE, 1);
        }
        else
        {
            Spell * starget = spell->getNextSpellTarget();
            starget->source->setColor(Constants::MTG_COLOR_BLUE, 1);
        }
        break;
    }

    case 2732: //Kjeldoran Frostbeast
    {
        observer->addObserver(NEW AKjeldoranFrostbeast(observer, _id, card));
        break;
    }

        // --- addon Mirage ---

    case 3410: //Seed of Innocence
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < observer->players[i]->game->inPlay->nb_cards; j++)
            {
                MTGCardInstance * current = observer->players[i]->game->inPlay->cards[j];
                if (current->hasType("Artifact"))
                {
                    observer->players[i]->game->putInGraveyard(current);
                    current->controller()->gainLife(current->getManaCost()->getConvertedCost(), card);
                }
            }
        }
        break;
    }

        //-- addon 10E---

    case 129767: //Threaten
    {
        observer->addObserver(NEW AInstantControlSteal(observer, _id, card, card->target));
        break;
    }
  
    case 129774: // Traumatize
    {
        int nbcards;
        Player * player = spell->getNextPlayerTarget();
        MTGLibrary * library = player->game->library;
        nbcards = (library->nb_cards) / 2;
        for (int i = 0; i < nbcards; i++)
        {
            if (library->nb_cards)
                player->game->putInZone(library->cards[library->nb_cards - 1], library, player->game->graveyard);
        }
        break;
    }
    case 130553:// Beacon of Immortality
    {
        Player * player = spell->getNextPlayerTarget();
        if (!player->inPlay()->hasAbility(Constants::CANTCHANGELIFE))
        {
            if (player->life < (INT_MAX / 4))
                player->life += player->life;
        }
        zones->putInZone(card, spell->from, zones->library);
        zones->library->shuffle();
        break;
    }
    case 135262:// Beacon of Destruction & unrest
    {
        zones->putInZone(card, spell->from, zones->library);
        zones->library->shuffle();
        break;
    }
    case 129750: //Sudden Impact
    {
        Damageable * target = spell->getNextDamageableTarget();
        Player * p = spell->getNextPlayerTarget();
        MTGHand * hand = p->game->hand;
        int damage = hand->nb_cards;
        observer->mLayers->stackLayer()->addDamage(card, target, damage);
        break;
    }
    case 130369: // Soulblast
    {
        int damage = 0;
        Damageable * target = spell->getNextDamageableTarget();
        for (int j = card->controller()->game->inPlay->nb_cards - 1; j >= 0; --j)
        {
            MTGCardInstance * current = card->controller()->game->inPlay->cards[j];
            if (current->hasType(Subtypes::TYPE_CREATURE))
            {
                card->controller()->game->putInGraveyard(current);
                damage += current->getCurrentPower();
            }
        }
        observer->mLayers->stackLayer()->addDamage(card, target, damage);
        break;
    }

    case 129698: // Reminisce
    {
        int nbcards;
        Player * player = spell->getNextPlayerTarget();
        MTGLibrary * library = player->game->library;
        MTGGraveyard * graveyard = player->game->graveyard;
        nbcards = (graveyard->nb_cards);
        for (int i = 0; i < nbcards; i++)
        {
            if (graveyard->nb_cards)
                player->game->putInZone(graveyard->cards[graveyard->nb_cards - 1], graveyard, library);
        }
        library->shuffle();
        break;
    }

        // --- addon Ravnica---

    case 89114: //Psychic Drain
    {
        Player * player = spell->getNextPlayerTarget();
        MTGLibrary * library = player->game->library;
        int x = computeX(spell, card);
        for (int i = 0; i < x; i++)
        {
            if (library->nb_cards)
                player->game->putInZone(library->cards[library->nb_cards - 1], library, player->game->graveyard);
        }
        observer->currentlyActing()->gainLife(x, card);
        break;
    }

    default:
        break;
    }

    /* We want to get rid of these basicAbility things.
     * basicAbilities themselves are alright, but creating New object depending on them is dangerous
     * The main reason is that classes that add an ability to a card do NOT create these objects, and therefore do NOT
     * Work.
     * For example, setting EXALTED for a creature is not enough right now...
     * It shouldn't be necessary to add an object. State based abilities could do the trick
     */

    if (card->basicAbilities[(int)Constants::EXALTED])
    {
        observer->addObserver(NEW AExalted(observer, _id, card));
    }

    if (card->basicAbilities[(int)Constants::FLANKING])
    {
        observer->addObserver(NEW AFlankerAbility(observer, _id, card));
    }

    if(card->basicAbilities[(int)Constants::MODULAR])
    {
        AModularAbility * ability = NEW AModularAbility(observer, _id, card, card, card->getModularValue());
        observer->addObserver(ability);
    }

    const int HomeAbilities[] = {(int)Constants::FORESTHOME, (int)Constants::ISLANDHOME, (int)Constants::MOUNTAINHOME, (int)Constants::SWAMPHOME, (int)Constants::PLAINSHOME};
    const char * HomeLands[] = {"forest", "island", "mountain", "swamp", "plains"};

    for (unsigned int i = 0; i < sizeof(HomeAbilities)/sizeof(HomeAbilities[0]); ++i)
    {
        if (card->basicAbilities[HomeAbilities[i]])
            observer->addObserver(NEW AStrongLandLinkCreature(observer, _id, card, HomeLands[i]));
    }

    if(card->previous && card->previous->previous && card->previous->previous->suspended)
        card->basicAbilities[(int)Constants::HASTE] = 1;

    if (card->hasType(Subtypes::TYPE_INSTANT) || card->hasType(Subtypes::TYPE_SORCERY))
    {
        MTGPlayerCards * zones = card->controller()->game;
        MTGPlayerCards * Endzones = card->owner->game;//put them in thier owners respective zones as per rules.
        if (card->basicAbilities[(int)Constants::EXILEDEATH] || card->basicAbilities[(int)Constants::GAINEDEXILEDEATH] || (card->basicAbilities[(int)Constants::HASDISTURB] && card->alternateCostPaid[ManaCost::MANA_PAID_WITH_RETRACE] == 1))
        {
            card->basicAbilities[(int)Constants::GAINEDEXILEDEATH] = 0;
            card->controller()->game->putInZone(card, card->getCurrentZone(), card->owner->game->exile);
        }
        else if (card->basicAbilities[(int)Constants::DOUBLEFACEDEATH] || card->basicAbilities[(int)Constants::GAINEDDOUBLEFACEDEATH])
        {
            card->basicAbilities[(int)Constants::GAINEDDOUBLEFACEDEATH] = 0;
            card->controller()->game->putInZone(card, card->getCurrentZone(), card->owner->game->temp);
        }
        else if (card->basicAbilities[(int)Constants::HANDDEATH] || card->basicAbilities[(int)Constants::GAINEDHANDDEATH])
        {
            card->basicAbilities[(int)Constants::GAINEDHANDDEATH] = 0;
            card->controller()->game->putInZone(card, card->getCurrentZone(), card->owner->game->hand);
        }
        else if (card->alternateCostPaid[ManaCost::MANA_PAID_WITH_BUYBACK] > 0)
        {
            card->alternateCostPaid[ManaCost::MANA_PAID_WITH_BUYBACK] = 0;
            zones->putInZone(card, zones->stack, Endzones->hand);
        }
        else if (card->alternateCostPaid[ManaCost::MANA_PAID_WITH_FLASHBACK] > 0)
        {
            zones->putInZone(card, zones->stack, Endzones->exile);
        }
        else
        {
            zones->putInZone(card, zones->stack, Endzones->graveyard);
        }
    }

}

//mehods used in parseMagicLine()

//ManaRedux -> manaredux(colorless,+2)
//          -> manaredux(green,-2)
MTGAbility * AbilityFactory::getManaReduxAbility(string s, int id, Spell *, MTGCardInstance *card, MTGCardInstance *target)
{
    int color = -1;
    string manaCost = s.substr(s.find(",") + 1);
    replace(manaCost.begin(), manaCost.end(), ')', ' ');
    trim(manaCost);
    const string ColorStrings[] = { Constants::kManaColorless, Constants::kManaGreen, Constants::kManaBlue, Constants::kManaRed, Constants::kManaBlack, Constants::kManaWhite };

    for (unsigned int i = 0; i < sizeof(ColorStrings)/sizeof(ColorStrings[0]); ++i)
    {
        if (s.find(ColorStrings[i]) != string::npos)
        {
            color = i;
            break;
        }
    }
    if (color == -1)
    {
        DebugTrace("An error has happened in creating a Mana Redux Ability! " << s );
        return NULL;
    }
    // figure out the mana cost
    int amount = 0;
    WParsedInt * value = NEW WParsedInt(manaCost, NULL, card);
    if(value){
        amount = value->getValue();
        SAFE_DELETE(value);
    }
    return NEW AAlterCost(observer, id, card, target, amount, color);
}

vector<void*> MTGAbility::deletedpointers;

MTGAbility::MTGAbility(const MTGAbility& a): ActionElement(a)
{
    //Todo get rid of menuText, it is only used as a placeholder in getMenuText, for something that could be a string
    for (int i = 0; i < 50; ++i)
    {
        menuText[i] = a.menuText[i];
    }

    game = a.game;

    oneShot = a.oneShot;
    forceDestroy = a.forceDestroy;
    forcedAlive = a.forcedAlive;
    canBeInterrupted = a.canBeInterrupted;

    //costs get copied, and will be deleted in the destructor
    mCost = a.mCost ? NEW ManaCost(a.mCost) : NULL;

    //alternative costs are not deleted in the destructor...who deletes them???
    alternative = a.alternative; // ? NEW ManaCost(a.alternative) : NULL;
    BuyBack = a.BuyBack; //? NEW ManaCost(a.BuyBack) : NULL;
    FlashBack = a.FlashBack; // ? NEW ManaCost(a.FlashBack) : NULL;
    Retrace = a.Retrace;// ? NEW ManaCost(a.Retrace) : NULL;
    Bestow = a.Bestow;
    morph =  a.morph;  //? NEW ManaCost(a.morph) : NULL;
    suspend = a.suspend;// ? NEW ManaCost(a.suspend) : NULL;

    //Those two are pointers, but we don't delete them in the destructor, no need to copy them
    target = a.target;
    source = a.source;

    aType = a.aType;
    naType = a.naType;
    abilitygranted = a.abilitygranted;
    
};

MTGAbility::MTGAbility(GameObserver* observer, int id, MTGCardInstance * card) :
    ActionElement(id)
{
    game = observer;
    source = card;
    target = card;
    aType = MTGAbility::UNKNOWN;
    mCost = NULL;
    forceDestroy = 0;
    forcedAlive = 0;
    oneShot = 0;
    canBeInterrupted = true;
}

MTGAbility::MTGAbility(GameObserver* observer, int id, MTGCardInstance * _source, Targetable * _target) :
    ActionElement(id)
{
    game = observer;
    source = _source;
    target = _target;
    aType = MTGAbility::UNKNOWN;
    mCost = NULL;
    forceDestroy = 0;
    forcedAlive = 0;
    oneShot = 0;
    canBeInterrupted = true;
}

void MTGAbility::setCost(ManaCost * cost, bool forceDelete)
{
    if (mCost) {
        DebugTrace("WARNING: Mtgability.cpp, attempt to set cost when previous cost is not null");
        if (forceDelete)
            delete(mCost);
    }
    mCost = cost;
}

int MTGAbility::stillInUse(MTGCardInstance * card)
{
    if (card == source || card == target)
        return 1;
    return 0;
}

MTGAbility::~MTGAbility()
{
    SAFE_DELETE(mCost);
}

int MTGAbility::addToGame()
{
    game->addObserver(this);
    return 1;
}

int MTGAbility::removeFromGame()
{
    game->removeObserver(this);
    return 1;
}

//returns 1 if this ability needs to be removed from the list of active abilities
int MTGAbility::testDestroy()
{
    if(waitingForAnswer)
        return 0;
    if(forceDestroy == 1)
        return 1;
    if(forceDestroy == -1)
        return 0;
    if(source->handEffects && game->isInHand(source))
        return 0;
    if(source->graveEffects && game->isInGrave(source))
        return 0;
    if(source->exileEffects && game->isInExile(source))
        return 0;
    if(source->commandZoneEffects && game->isInCommandZone(source))
        return 0;
    if(forcedAlive == 1)
        return 0;
    if(game->mLayers->stackLayer()->has(this)) //Moved here to avoid a random crash (e.g. blasphemous act)
        return 0;
    if(!game->isInPlay(source))
        return 1;
    if(target && !dynamic_cast<Player*>(target) && !game->isInPlay((MTGCardInstance *) target))
        return 1;
    return 0;
}

int MTGAbility::fireAbility()
{
    if (canBeInterrupted)
        game->mLayers->stackLayer()->addAbility(this);
    else
        resolve();
    return 1;
}

ostream& MTGAbility::toString(ostream& out) const
{
    return out << "MTGAbility ::: menuText : " << menuText << " ; game : " << game << " ; forceDestroy : " << forceDestroy
                    << " ; mCost : " << mCost << " ; target : " << target << " ; aType : " << aType << " ; source : " << source;
}

Player * MTGAbility::getPlayerFromTarget(Targetable * target)
{
    if (!target)
        return NULL;

    if (MTGCardInstance * cTarget = dynamic_cast<MTGCardInstance *>(target))
        return cTarget->controller();

    if (Player * cPlayer = dynamic_cast<Player *>(target))
        return cPlayer;

    return ((Interruptible *) target)->source->controller();
}

Player * MTGAbility::getPlayerFromDamageable(Damageable * target)
{
    if (!target)
        return NULL;

    if (target->type_as_damageable == Damageable::DAMAGEABLE_MTGCARDINSTANCE)
        return ((MTGCardInstance *) target)->controller();

    return (Player *) target;
}

//

NestedAbility::NestedAbility(MTGAbility * _ability)
{
    ability = _ability;
}

//

ActivatedAbility::ActivatedAbility(GameObserver* observer, int id, MTGCardInstance * card, ManaCost * _cost, int restrictions,string limit,MTGAbility * sideEffect,string usesBeforeSideEffects,string castRestriction) :
    MTGAbility(observer, id, card), restrictions(restrictions), needsTapping(0),limit(limit),sideEffect(sideEffect),usesBeforeSideEffects(usesBeforeSideEffects),castRestriction(castRestriction)
{
    counters = 0;
    setCost(_cost);
    abilityCost = 0;
    sa = NULL;
}

int ActivatedAbility::isReactingToClick(MTGCardInstance * card, ManaCost * mana)
{        
    if(card->isPhased)
        return 0;
    Player * player = game->currentlyActing();
    int cPhase = game->getCurrentGamePhase();
    switch (restrictions)
    {
    case PLAYER_TURN_ONLY:
        if (player != game->currentPlayer)
            return 0;
        break;
    case OPPONENT_TURN_ONLY:
        if (player == game->currentPlayer)
            return 0;
        break;
    case AS_SORCERY:
        if (player != game->currentPlayer)
            return 0;
        if (!card->hasType(Subtypes::TYPE_EQUIPMENT) && cPhase != MTG_PHASE_FIRSTMAIN && cPhase != MTG_PHASE_SECONDMAIN)
            return 0;
        if (card->hasType(Subtypes::TYPE_EQUIPMENT) && !card->has(Constants::EQPASINST) && cPhase != MTG_PHASE_FIRSTMAIN && cPhase != MTG_PHASE_SECONDMAIN)
            return 0;
        if (player->opponent()->getObserver()->mLayers->stackLayer()->count(0, NOT_RESOLVED) != 0||game->mLayers->stackLayer()->count(0, NOT_RESOLVED) != 0||player->getObserver()->mLayers->stackLayer()->count(0, NOT_RESOLVED) != 0)
            return 0;
        break;
    }
    if (restrictions >= MY_BEFORE_BEGIN && restrictions <= MY_AFTER_EOT)
    {
        if (player != game->currentPlayer)
            return 0;
        if (cPhase != restrictions - MY_BEFORE_BEGIN + MTG_PHASE_BEFORE_BEGIN)
            return 0;
    }

    if (restrictions >= OPPONENT_BEFORE_BEGIN && restrictions <= OPPONENT_AFTER_EOT)
    {
        if (player == game->currentPlayer)
            return 0;
        if (cPhase != restrictions - OPPONENT_BEFORE_BEGIN + MTG_PHASE_BEFORE_BEGIN)
            return 0;
    }

    if (restrictions >= BEFORE_BEGIN && restrictions <= AFTER_EOT)
    {
        if (cPhase != restrictions - BEFORE_BEGIN + MTG_PHASE_BEFORE_BEGIN)
            return 0;
    }
    limitPerTurn = 0;
    if(limit.size())
    {
        WParsedInt * value = NEW WParsedInt(limit.c_str(),NULL,source);
        limitPerTurn = value->getValue();
        delete value;
        //only run this check if we have a valid limit string.
        //limits on uses are based on when the ability is used, not when it is resolved
        //incrementing of counters directly after the fireability()
        //as ability limits are supposed to count the use regaurdless of the ability actually
        //resolving. this check was previously located in genericactivated, and incrementing was handled in the resolve.
        if (limitPerTurn && counters >= limitPerTurn)
            return 0;
    }
    if(castRestriction.size())
    {
        AbilityFactory af(game);
        int checkCond = af.parseCastRestrictions(card,card->controller(),castRestriction);
        if(!checkCond)
            return 0;
    }
    if (card == source && source->controller() == player && (!needsTapping || (!source->isTapped()
                    && !source->hasSummoningSickness())))
    {
        ManaCost * cost = getCost();
        if (!cost)
            return 1;
        if(card->hasType(Subtypes::TYPE_PLANESWALKER))
        {
            // Improved the check to avoid the multiple triggers in case of abilities gained from other cards (e.g. Kasmina, Enigma Sage)
            bool turnSide = false;
            int howMany = 0;
            for(unsigned int k = 0; k < card->getObserver()->mLayers->actionLayer()->mObjects.size(); ++k)
            {
                ActivatedAbility * check = dynamic_cast<ActivatedAbility*>(card->getObserver()->mLayers->actionLayer()->mObjects[k]);
                turnSide = card->isFlipped > 0 && !card->isInPlay(card->getObserver());
                if(!turnSide && check && check->source == card){
                    if(check->counters && !card->has(Constants::CANLOYALTYTWICE))
                        return 0;
                    howMany += check->counters;
                }
            }
            if(howMany > 1)
                return 0;
            if (player != game->currentPlayer && !card->has(Constants::CANLOYALTYASINST))
                return 0;
            if (!turnSide && !card->has(Constants::CANLOYALTYASINST) && (cPhase != MTG_PHASE_FIRSTMAIN && cPhase != MTG_PHASE_SECONDMAIN))
                return 0;
        }
        if (source->has(Constants::NOACTIVATED) || (source->mutation && source->parentCards.size() > 0)) // Mutated Over/Under card doesn't have to react to click anymore
            return 0;
        AbilityFactory af(game);
        MTGAbility * fmp = NULL;
        fmp = af.getCoreAbility(this);
        AManaProducer * amp = dynamic_cast<AManaProducer *> (this);
        AManaProducer * femp = dynamic_cast<AManaProducer *> (fmp);
        if (source->has(Constants::NOMANA) && (amp||femp))
            return 0;
        if(source->has(Constants::ONLYMANA) && !(amp||femp))
            return 0;
        cost->setExtraCostsAction(this, card);
        if (source->has(Constants::NOACTIVATEDTAP) && cost->extraCosts)
        {
            for(unsigned int i = 0;i < cost->extraCosts->costs.size();++i)
            {
                ExtraCost * eCost = cost->getExtraCost(i);
                if(dynamic_cast<TapCost *>(eCost))
                {
                    return 0;
                }
            }
        }
        if (!mana)
            mana = player->getManaPool();
        if (!mana->canAfford(cost,card->has(Constants::ANYTYPEOFMANAABILITY)))
            return 0;
        if (!cost->canPayExtra())
            return 0;
        return 1;
    }
    return 0;
}

int ActivatedAbility::reactToClick(MTGCardInstance * card)
{
    if (!isReactingToClick(card))
        return 0;
    Player * player = game->currentlyActing();
    ManaCost * cost = getCost();
    if (cost)
    {
        if (!cost->isExtraPaymentSet())
        {
            game->mExtraPayment = cost->extraCosts;
            return 0;
        }
        if(cost->extraCosts){ // Added to check if the snow mana amount is enough to pay all the snow cost.
            int countSnow = 0;
            for(unsigned int i = 0; i < cost->extraCosts->costs.size(); i++){
                if(dynamic_cast<SnowCost*> (cost->extraCosts->costs[i]))
                    countSnow++;
            }
            if((source->controller()->snowManaG + source->controller()->snowManaU + source->controller()->snowManaR + 
                source->controller()->snowManaB + source->controller()->snowManaW + source->controller()->snowManaC) < countSnow){
                game->mExtraPayment = cost->extraCosts;
                return 0;
            }
        }
        ManaCost * previousManaPool = NEW ManaCost(player->getManaPool());
        cost->doPayExtra(); // Bring here brefore the normal payment to solve Snow Mana payment bug.
        game->currentlyActing()->getManaPool()->pay(cost);
        SAFE_DELETE(abilityCost);
        abilityCost = previousManaPool->Diff(player->getManaPool());
        delete previousManaPool;
    }
    return ActivatedAbility::activateAbility();
}

int ActivatedAbility::reactToTargetClick(Targetable * object)
{
    if (!isReactingToTargetClick(object))
        return 0;
    Player * player = game->currentlyActing();
    ManaCost * cost = getCost();
    if (cost)
    {
        if (MTGCardInstance * cObject = dynamic_cast<MTGCardInstance *>(object))
            cost->setExtraCostsAction(this, cObject);
        if (!cost->isExtraPaymentSet())
        {
            game->mExtraPayment = cost->extraCosts;
            return 0;
        }
        if(cost->extraCosts){ // Added to check if the snow mana amount is enough to pay all the snow cost.
            int countSnow = 0;
            for(unsigned int i = 0; i < cost->extraCosts->costs.size(); i++){
                if(dynamic_cast<SnowCost*> (cost->extraCosts->costs[i]))
                    countSnow++;
            }
            if((source->controller()->snowManaG + source->controller()->snowManaU + source->controller()->snowManaR + 
                source->controller()->snowManaB + source->controller()->snowManaW + source->controller()->snowManaC) < countSnow){
                game->mExtraPayment = cost->extraCosts;
                return 0;
            }
        }
        ManaCost * previousManaPool = NEW ManaCost(player->getManaPool());
        cost->doPayExtra(); // Bring here brefore the normal payment to solve Snow Mana payment bug.
        game->currentlyActing()->getManaPool()->pay(cost);
        SAFE_DELETE(abilityCost);
        abilityCost = previousManaPool->Diff(player->getManaPool());
        delete previousManaPool;
    }
    return ActivatedAbility::activateAbility();
}

int ActivatedAbility::activateAbility()
{
    MTGAbility * fmp = NULL;
    bool wasTappedForMana = false;
    //taking foreach manaproducers off the stack and sending tapped for mana events.
    AbilityFactory af(game);
    fmp = af.getCoreAbility(this);
    AManaProducer * amp = dynamic_cast<AManaProducer *> (this);
    AManaProducer * femp = dynamic_cast<AManaProducer *> (fmp);
    ManaCost * cost = getCost();
    if((amp||femp) && cost && cost->extraCosts)
    {
        for(unsigned int i = 0; i < cost->extraCosts->costs.size();i++)
        {
            ExtraCost * tapper = dynamic_cast<TapCost*>(cost->extraCosts->costs[i]);
            if(tapper)
                needsTapping = 1;
            wasTappedForMana = true;
        }
    }
    else if(amp||femp)
    {
        if(amp)
            needsTapping = amp->tap;
        else
            needsTapping = femp->tap;
    }
    if (needsTapping && (source->isInPlay(game)|| wasTappedForMana))
    {
        if (amp||femp)
        {
            WEvent * e = NEW WEventCardTappedForMana(source, 0, 1);
            game->receiveEvent(e);
        }
    }
    if (amp||femp)
    {
        counters++;
        if(sideEffect && usesBeforeSideEffects.size())
        {
            activateSideEffect();
        }
        this->resolve();
        return 1;
    }
    counters++;
    if(sideEffect && usesBeforeSideEffects.size())
    {
        activateSideEffect();
    }
    fireAbility();
    return 1;
}

void ActivatedAbility::activateSideEffect()
{
    WParsedInt * use = NEW WParsedInt(usesBeforeSideEffects.c_str(),NULL,source);
    uses = use->getValue();
    delete use;
    if(counters == uses)
    {
        sa = sideEffect->clone();
        sa->target = this->target;
        sa->source = this->source;
        if(sa->oneShot)
        {
            sa->fireAbility();
        }
        else
        {
            GenericInstantAbility * wrapper = NEW GenericInstantAbility(game, 1, source, (Damageable *) (this->target), sa);
            wrapper->addToGame();
        }
    }
    return;
}

ActivatedAbility::~ActivatedAbility()
{
    SAFE_DELETE(abilityCost);
    SAFE_DELETE(sideEffect);
    SAFE_DELETE(sa);
}

ostream& ActivatedAbility::toString(ostream& out) const
{
    out << "ActivatedAbility ::: restrictions : " << restrictions << " ; needsTapping : " << needsTapping << " (";
    return MTGAbility::toString(out) << ")";
}

TargetAbility::TargetAbility(GameObserver* observer, int id, MTGCardInstance * card, TargetChooser * _tc, ManaCost * _cost, int _playerturnonly, string castRestriction) :
    ActivatedAbility(observer, id, card, _cost, _playerturnonly, "", NULL, "", castRestriction), NestedAbility(NULL) //Todo fix this mess, why do we have to pass "", NULL, "" here before cast restrictions?
{
    tc = _tc;
}

TargetAbility::TargetAbility(GameObserver* observer, int id, MTGCardInstance * card, ManaCost * _cost, int _playerturnonly, string castRestriction) :
    ActivatedAbility(observer, id, card, _cost, _playerturnonly,  "", NULL, "", castRestriction), NestedAbility(NULL) //Todo fix this mess, why do we have to pass "", NULL, "" here before cast restrictions?
{
    tc = NULL;
}

int TargetAbility::reactToTargetClick(Targetable * object)
{
    if (MTGCardInstance * cObject = dynamic_cast<MTGCardInstance *>(object))
        return reactToClick(cObject);

    if (waitingForAnswer)
    {
        if (tc->toggleTarget(object) == TARGET_OK_FULL)
        {
            waitingForAnswer = 0;
            game->mLayers->actionLayer()->setCurrentWaitingAction(NULL);
            return ActivatedAbility::reactToClick(source);
        }
        return 1;
    }
    return 0;
}

int TargetAbility::reactToClick(MTGCardInstance * card)
{
    if (!waitingForAnswer)
    {
        if (isReactingToClick(card))
        {
            ManaCost * cost = getCost();
            if (cost && !cost->isExtraPaymentSet())
            {
                game->mExtraPayment = cost->extraCosts;
                return 0;
            }
            if (!tc) return 0; // Fix crash on mutating cards
            waitingForAnswer = 1;
            game->mLayers->actionLayer()->setCurrentWaitingAction(this);
            tc->initTargets();
            return 1;
        }
    }
    else
    {
        if (card == source && ((tc->targetsReadyCheck() == TARGET_OK && !tc->targetMin) || tc->targetsReadyCheck() == TARGET_OK_FULL))
        {
            waitingForAnswer = 0;
            game->mLayers->actionLayer()->setCurrentWaitingAction(NULL);
            return ActivatedAbility::reactToClick(source);
        }
        else
        {
            if (tc->toggleTarget(card) == TARGET_OK_FULL && tc->targetsReadyCheck() == TARGET_OK_FULL)
            {
                int result = ActivatedAbility::reactToClick(source);
                if (result)
                {
                    waitingForAnswer = 0;
                    game->mLayers->actionLayer()->setCurrentWaitingAction(NULL);
                    if(tc->targetter)
                    {
                    WEvent * e = NEW WEventTarget(card,source);
                    game->receiveEvent(e);
                    }
                }
                return result;
            }
            return 1;
        }
    }
    return 0;
}

void TargetAbility::Render()
{
    //TODO ?
}

int TargetAbility::resolve()
{
    Targetable * t = tc->getNextTarget();
    if (t && ability)
    {
        if (abilityCost)
        {
            source->X = 0;
            ManaCost * diff = abilityCost->Diff(getCost());
            source->X = diff->hasX();
            delete (diff);
        }

        ability->target = t;
        //do nothing if the target controller responded by phasing out the target.
        if (dynamic_cast<MTGCardInstance *>(t) && ((MTGCardInstance*)t)->isPhased)
            return 0;
        Player * targetedPlyr = dynamic_cast<Player *>(t);
        if (targetedPlyr)
        {
            source->playerTarget = targetedPlyr;
        }
        if (ability->oneShot)
        {
            while(t)
            {
                ability->resolve();
                t = tc->getNextTarget(t);
                ability->target = t;
            }
            tc->initTargets();
            return 1;
        }
        else
        {
            while(t)
            {
                MTGAbility * a = ability->clone();
                a->addToGame();
                t = tc->getNextTarget(t);
                ability->target = t;
            }
            tc->initTargets();
            return 1;
        }
    }
    return 0;
}

const string TargetAbility::getMenuText()
{
    if (ability)
        return ability->getMenuText();
    return ActivatedAbility::getMenuText();
}

TargetAbility::~TargetAbility()
{
    SAFE_DELETE(ability);
}

ostream& TargetAbility::toString(ostream& out) const
{
    out << "TargetAbility ::: (";
    return ActivatedAbility::toString(out) << ")";
}

//


TriggeredAbility::TriggeredAbility(GameObserver* observer, int id, MTGCardInstance * card, Targetable * _target) :
    MTGAbility(observer, id, card, _target)
{
}

TriggeredAbility::TriggeredAbility(GameObserver* observer, int id, MTGCardInstance * card) :
    MTGAbility(observer, id, card)
{
}

int TriggeredAbility::receiveEvent(WEvent * e)
{
    if (triggerOnEvent(e))
    {
    if(dynamic_cast<WEventTarget*>(e))
    {
        //@targetted trigger as per mtg rules is a state based trigger
        //that resolves instantly before the event that targetted it.
        resolve();
        return 1;
    }
    if(dynamic_cast<WEventCardSacrifice*>(e))
    {
        //sacrificed event
        //thraximundar vs bloodfore collosus, thraximundar 
        //must be able to survive a sacrificed bloodfire collosus,
        //same with mortician beetle vs phyrexian denouncer test
        resolve();
        return 1;
    }
    if(dynamic_cast<WEventCardExploited*>(e))
    {
        //exploited event must resolve instantly or by the time they do the cards that triggered them 
        //have already been put in graveyard.
        resolve();
        return 1;
    }
    if(dynamic_cast<WEventCardDiscard*>(e))
    {
        //discard event must resolve instantly or by the time they do the cards that triggered them 
        //have already been put in graveyard.
        resolve();
        return 1;
    }
    WEventCardCycle * cycleCheck = dynamic_cast<WEventCardCycle*>(e);
    if(cycleCheck && cycleCheck->card == source)
    {
        resolve();
        return 1;
        //When you cycle this card, first the cycling ability goes on the stack, 
        //then the triggered ability goes on the stack on top of it. 
        //The triggered ability will resolve before you draw a card from the cycling ability.
        //
        //The cycling ability and the triggered ability are separate. 
        //If the triggered ability is countered (with Stifle, for example, or if all its targets have become illegal), 
        //the cycling ability will still resolve and you'll draw a card.
    }
    WEventZoneChange * stackCheck = dynamic_cast<WEventZoneChange*>(e);
    if(stackCheck && (stackCheck->to == game->currentPlayer->game->stack||stackCheck->to == game->currentPlayer->opponent()->game->stack))
    {
        resolve();
        return 1;
        //triggers that resolve from stack events must resolve instantly or by the time they do the cards that triggered them 
        //have already been put in play or graveyard.
    }
        fireAbility();
        return 1;
    }
    return 0;
}

void TriggeredAbility::Update(float)
{
    if (trigger())
        fireAbility();
}

ostream& TriggeredAbility::toString(ostream& out) const
{
    out << "TriggeredAbility ::: (";
    return MTGAbility::toString(out) << ")";
}

// Trigger
Trigger::Trigger(GameObserver* observer, int id, MTGCardInstance * source, bool once, TargetChooser * _tc)
    : TriggeredAbility(observer, id, source), mOnce(once), mActiveTrigger(true)
{
    tc = _tc;
}

int  Trigger::triggerOnEvent(WEvent * event) {
    if(!mActiveTrigger) 
        return 0;

    //Abilities don't work if the card is phased
    if(source->isPhased) return 0;

    if (!triggerOnEventImpl(event))
        return 0;

    if(mOnce && mActiveTrigger)
        mActiveTrigger = false;

    if(castRestriction.size())
    {
        if(!source)
            return 1;//can't check these restrictions without a source aka:in a rule.txt
        AbilityFactory af(game);
        int checkCond = af.parseCastRestrictions(source,source->controller(),castRestriction);

        if(!checkCond)
            return 0;
    }
    return 1;
}

//
InstantAbility::InstantAbility(GameObserver* observer, int _id, MTGCardInstance * source) :
    MTGAbility(observer, _id, source)
{
    init = 0;
}

void InstantAbility::Update(float)
{
    if (!init)
    {
        init = resolve();
    }
}

InstantAbility::InstantAbility(GameObserver* observer, int _id, MTGCardInstance * source, Targetable * _target) :
    MTGAbility(observer, _id, source, _target)
    {
        init = 0;
    }

//Instant abilities last generally until the end of the turn
    int InstantAbility::testDestroy()
    {
        GamePhase newPhase = game->getCurrentGamePhase();
        if (newPhase != currentPhase && newPhase == MTG_PHASE_AFTER_EOT)
            return 1;
        currentPhase = newPhase;
        return 0;
    }

ostream& InstantAbility::toString(ostream& out) const
{
    out << "InstantAbility ::: init : " << init << " (";
    return MTGAbility::toString(out) << ")";
}

bool ListMaintainerAbility::canTarget(MTGGameZone * zone)
{
    if (tc)
        return tc->targetsZone(zone);
    for (int i = 0; i < 2; i++)
    {
        Player * p = game->players[i];
        if (zone == p->game->inPlay)
            return true;
    }
    return false;
}

void ListMaintainerAbility::updateTargets()
{
    //remove invalid ones
    map<MTGCardInstance *, bool> temp;
    for (map<MTGCardInstance *, bool>::iterator it = cards.begin(); it != cards.end(); ++it)
    {
        MTGCardInstance * card = (*it).first;
        if (!canBeInList(card) || card->mPropertiesChangedSinceLastUpdate)
        {
            temp[card] = true;
        }
    }

    for (map<MTGCardInstance *, bool>::iterator it = temp.begin(); it != temp.end(); ++it)
    {
        MTGCardInstance * card = (*it).first;
        cards.erase(card);
        if(!card->mutation) // Fix crash on mutating card...
            removed(card);
    }
    temp.clear();
    //add New valid ones
    for (int i = 0; i < 2; i++)
    {
        Player * p = game->players[i];
        MTGGameZone * zones[] = { p->game->inPlay, p->game->graveyard, p->game->hand, p->game->library, p->game->stack, p->game->exile, p->game->commandzone, p->game->sideboard, p->game->reveal };
        for (int k = 0; k < 9; k++)
        {
            MTGGameZone * zone = zones[k];
            if (canTarget(zone))
            {
                for (int j = 0; j < zone->nb_cards; j++)
                {
                     MTGCardInstance * card = zone->cards[j];
                    if (canBeInList(card))
                    {
                        if (cards.find(card) == cards.end())
                        {
                            temp[card] = true;
                        }
                    }
                }
            }
        }
    }

    for (map<MTGCardInstance *, bool>::iterator it = temp.begin(); it != temp.end(); ++it)
    {
        MTGCardInstance * card = (*it).first;
        cards[card] = true;
        added(card);
    }

    temp.clear();

    for (int i = 0; i < 2; ++i)
    {
        Player * p = game->players[i];
        if (!players[p] && canBeInList(p))
        {
            players[p] = true;
            added(p);
        }
        else if (players[p] && !canBeInList(p))
        {
            players[p] = false;
            removed(p);
        }
    }

}

void ListMaintainerAbility::checkTargets()
{
    //remove invalid ones
    map<MTGCardInstance *, bool> tempCheck;
    for (map<MTGCardInstance *, bool>::iterator it = checkCards.begin(); it != checkCards.end(); ++it)
    {
        MTGCardInstance * card = (*it).first;
        if (!canBeInList(card) || card->mPropertiesChangedSinceLastUpdate)
        {
            tempCheck[card] = true;
        }
    }

    for (map<MTGCardInstance *, bool>::iterator it = tempCheck.begin(); it != tempCheck.end(); ++it)
    {
        MTGCardInstance * card = (*it).first;
        checkCards.erase(card);
    }

    tempCheck.clear();

    //add New valid ones
    for (int i = 0; i < 2; i++)
    {
        Player * p = game->players[i];
        MTGGameZone * zones[] = { p->game->inPlay, p->game->graveyard, p->game->hand, p->game->library, p->game->stack, p->game->exile, p->game->commandzone, p->game->sideboard, p->game->reveal };
        for (int k = 0; k < 9; k++)
        {
            MTGGameZone * zone = zones[k];
            if (canTarget(zone))
            {
                for (int j = 0; j < zone->nb_cards; j++)
                {
                     MTGCardInstance * card = zone->cards[j];
                    if (canBeInList(card))
                    {
                        if (checkCards.find(card) == checkCards.end())
                        {
                            tempCheck[card] = true;
                        }
                    }
                }
            }
        }
    }
    for (map<MTGCardInstance *, bool>::iterator it = tempCheck.begin(); it != tempCheck.end(); ++it)
    {
        MTGCardInstance * card = (*it).first;
        checkCards[card] = true;
    }
}

void ListMaintainerAbility::Update(float)
{
    updateTargets();
}

//Destroy the spell -> remove all targets
int ListMaintainerAbility::destroy()
{
    map<MTGCardInstance *, bool>::iterator it = cards.begin();

    while (it != cards.end())
    {
        MTGCardInstance * card = (*it).first;
        cards.erase(card);
        if(!card->mutation) // Fix crash on mutating card...
            removed(card);
        it = cards.begin();
    }
    return 1;
}

ostream& ListMaintainerAbility::toString(ostream& out) const
{
    out << "ListMaintainerAbility ::: (";
    return MTGAbility::toString(out) << ")";
}

TriggerAtPhase::TriggerAtPhase(GameObserver* observer, int id, MTGCardInstance * source, Targetable * target, int _phaseId, int who, bool sourceUntapped, bool sourceTap, bool lifelost, int lifeamount, bool once) :
    TriggeredAbility(observer, id, source, target), phaseId(_phaseId), who(who), sourceUntapped(sourceUntapped), sourceTap(sourceTap),lifelost(lifelost),lifeamount(lifeamount),once(once)
{
    activeTrigger = true;
    if (game)
    {
        newPhase = game->getCurrentGamePhase();
        currentPhase = newPhase;
    }
}

    int TriggerAtPhase::trigger()
    {
        if(!activeTrigger) return 0;
        if(source->isPhased) return 0;
        if(lifelost)
        {
            int lifeloss = source->controller()->opponent()->lifeLostThisTurn;
            if(lifeloss < lifeamount)
                return 0;
        }
        if (sourceUntapped  && source->isTapped() == 1)
            return 0;
        if (sourceTap  && !source->isTapped())
            return 0;
        if (testDestroy())
            return 0; // http://code.google.com/p/wagic/issues/detail?id=426
        int result = 0;
        if (currentPhase != newPhase && newPhase == phaseId)
        {
        result = 0;
        switch (who)
        {
        case 1:
            if (game->currentPlayer == source->controller())
                result = 1;
            break;
        case -1:
            if (game->currentPlayer != source->controller())
                result = 1;
            break;
        case -2:
            if (source->target)
            {
                if (game->currentPlayer == source->target->controller())
                    result = 1;
            }
            else
            {
                if (game->currentPlayer == source->controller())
                    result = 1;
            }
            break;
        case -3:
            if (source->playerTarget)
            {
                if (game->currentPlayer == source->playerTarget)
                    result = 1;
            }
            break;
        default:
            result = 1;
            break;
        }
        if(castRestriction.size())
        {
            if(!source)
                result = 1;//can't check these restrictions without a source aka:in a rule.txt
            AbilityFactory af(game);
            int checkCond = af.parseCastRestrictions(source,source->controller(),castRestriction);
            if(!checkCond)
                result = 0;
        }

    }
        if(once && activeTrigger)
            activeTrigger = false;

    return result;
}

TriggerAtPhase* TriggerAtPhase::clone() const
{
    return NEW TriggerAtPhase(*this);
}

TriggerNextPhase::TriggerNextPhase(GameObserver* observer, int id, MTGCardInstance * source, Targetable * target, int _phaseId, int who, bool sourceUntapped, bool sourceTap, bool once) :
    TriggerAtPhase(observer, id, source, target, _phaseId, who, sourceUntapped, sourceTap, once)
{
    destroyActivated = 0;
    activeTrigger = true;
}

int TriggerNextPhase::testDestroy()
{
    //dirty hack because of http://code.google.com/p/wagic/issues/detail?id=426
    if (newPhase <= phaseId && !destroyActivated)
        destroyActivated = 1;
    if (destroyActivated > 1 || (newPhase > phaseId && destroyActivated))
    {
        destroyActivated++;
        return 1;
    }
    return 0;
}

TriggerNextPhase* TriggerNextPhase::clone() const
{
    return NEW TriggerNextPhase(*this);
}

TriggerRebound::TriggerRebound(GameObserver* observer, int id, MTGCardInstance * source, Targetable * target, int _phaseId, int who, bool sourceUntapped, bool sourceTap, bool once) :
    TriggerAtPhase(observer, id, source, target, _phaseId, who, sourceUntapped, sourceTap, once)
{
    destroyActivated = 0;
    activeTrigger = true;
}

int TriggerRebound::testDestroy()
{
    if(newPhase <= phaseId && !destroyActivated && game->currentPlayer == source->controller())
        destroyActivated=1;
    if(destroyActivated > 1||(newPhase > phaseId && destroyActivated))
    {
        destroyActivated++;
        return 1;
    }
    return 0;
}

TriggerRebound* TriggerRebound::clone() const
{
    return NEW TriggerRebound(*this);
}

GenericTriggeredAbility::GenericTriggeredAbility(GameObserver* observer, int id, MTGCardInstance * _source, TriggeredAbility * _t, MTGAbility * a,
                MTGAbility * dc, Targetable * _target) :
    TriggeredAbility(observer, id, _source, _target), NestedAbility(a)
{
    if (!target)
        target = source;
    t = _t;
    destroyCondition = dc;

    t->source = source;
    t->target = target;
    ability->source = source;
    ability->target = target;
    if (destroyCondition)
    {
        destroyCondition->source = source;
        destroyCondition->target = target;
    }
}

int GenericTriggeredAbility::trigger()
{
    return t->trigger();
}

int GenericTriggeredAbility::triggerOnEvent(WEvent * e)
{
    if (t->triggerOnEvent(e))
    {
        targets.push(getTriggerTarget(e, ability));
        return 1;
    }
    return 0;
}

Targetable * GenericTriggeredAbility::getTriggerTarget(WEvent * e, MTGAbility * a)
{
    TriggerTargetChooser * ttc = dynamic_cast<TriggerTargetChooser *> (a->getActionTc());
    if (ttc)
        return e->getTarget(ttc->triggerTarget);

    NestedAbility * na = dynamic_cast<NestedAbility *> (a);
    if (na)
        return getTriggerTarget(e, na->ability);

    MultiAbility * ma = dynamic_cast<MultiAbility *> (a);
    if (ma)
    {
        for (size_t i = 0; i < ma->abilities.size(); i++)
        {
            return getTriggerTarget(e, ma->abilities[i]);
        }
    }

    return NULL;
}

void GenericTriggeredAbility::setTriggerTargets(Targetable * ta, MTGAbility * a)
{
    TriggerTargetChooser * ttc = dynamic_cast<TriggerTargetChooser *> (a->getActionTc());
    if (ttc)
    {
        a->target = ta;
        ttc->target = ta;
    }

    NestedAbility * na = dynamic_cast<NestedAbility *> (a);
    if (na)
        setTriggerTargets(ta, na->ability);

    MultiAbility * ma = dynamic_cast<MultiAbility *> (a);
    if (ma)
    {
        for (size_t i = 0; i < ma->abilities.size(); i++)
        {
            setTriggerTargets(ta, ma->abilities[i]);
        }
    }
}

void GenericTriggeredAbility::Update(float dt)
{
    GamePhase newPhase = game->getCurrentGamePhase();
    t->newPhase = newPhase;
    TriggeredAbility::Update(dt);
    t->currentPhase = newPhase;
}

int GenericTriggeredAbility::resolve()
{
    if(source->isPhased) return 0;
    if (targets.size())
    {
        setTriggerTargets(targets.front(), ability);
        targets.pop();
    }
    if (ability->oneShot)
        return ability->resolve();
    MTGAbility * clone = ability->clone();
    clone->addToGame();
    return 1;
}

int GenericTriggeredAbility::testDestroy()
{
    if (!TriggeredAbility::testDestroy())
        return 0;
    if (destroyCondition)
        return (destroyCondition->testDestroy());
    return t->testDestroy();
}

GenericTriggeredAbility::~GenericTriggeredAbility()
{
    delete t;
    delete ability;
    SAFE_DELETE(destroyCondition);
}

const string GenericTriggeredAbility::getMenuText()
{
    return ability->getMenuText();
}

GenericTriggeredAbility* GenericTriggeredAbility::clone() const
{
    GenericTriggeredAbility * a =  NEW GenericTriggeredAbility(*this);
    a->t = t->clone();
    a->ability = ability->clone();
    a->destroyCondition = destroyCondition->clone();
    return a;
}

/*Mana Producers (lands)
 //These have a reactToClick function, and therefore two manaProducers on the same card conflict with each other
 //That means the player has to choose one. although that is perfect for cards such as birds of paradise or badlands,
 other solutions need to be provided for abilities that add mana (ex: mana flare)
 */

AManaProducer::AManaProducer(GameObserver* observer, int id, MTGCardInstance * card, Targetable * t, ManaCost * _output, ManaCost * _cost,
                int who,string producing, bool doesntEmpty) :
    ActivatedAbilityTP(observer, id, card, t, _cost, who)
{

    aType = MTGAbility::MANA_PRODUCER;
    setCost(_cost);
    output = _output;
    tap = 0;
    Producing = producing;
    menutext = "";
    DoesntEmpty = doesntEmpty;
    andAbility = NULL;
}

int AManaProducer::isReactingToClick(MTGCardInstance * _card, ManaCost * mana)
{
    int result = 0;
    if (!mana)
        mana = game->currentlyActing()->getManaPool();
    //please do not condense the following, I broke it apart for readability, it was far to difficult to tell what exactly happened before with it all in a single line.
    //and far to prone to bugs.
    if (_card == source)
    {
        if (!tap || (tap && (!source->isTapped() && !source->hasSummoningSickness())))
        {
            if (game->currentlyActing()->game->inPlay->hasCard(source) && (source->hasType(Subtypes::TYPE_LAND) || !tap || !source->hasSummoningSickness()))
            {
                if (!source->isPhased)
                {
                    ManaCost * cost = getCost();
                    if (!cost || (mana->canAfford(cost,_card->has(Constants::ANYTYPEOFMANAABILITY)) && (!cost->extraCosts || cost->extraCosts->canPay())))/*counter cost bypass react to click*/
                    {
                        result = 1;
                    }
                }
            }
        }
    }
    return result;
}

int AManaProducer::resolve()
{
    Targetable * _target = getTarget();
    Player * player = getPlayerFromTarget(_target);
    if (!player)
        return 0;
    
    player->getManaPool()->add(output, source);
    if(DoesntEmpty)
        player->doesntEmpty->add(output);

    if(source->name.empty() && source->storedSourceCard){ // Fix for mana produced inside ability$!!$ keyword.
        source->storedSourceCard->getProducedMana()->copy(output);
        WEventCardManaProduced * ev = NEW WEventCardManaProduced(source->storedSourceCard);
        if(ev)
            source->storedSourceCard->getObserver()->receiveEvent(ev);
    } else {
        source->getProducedMana()->copy(output);
        WEventCardManaProduced * ev = NEW WEventCardManaProduced(source);
        if(ev)
            source->getObserver()->receiveEvent(ev);
    }

    if(andAbility)
    {
        MTGAbility * andAbilityClone = andAbility->clone();
        andAbilityClone->target = source;
        if(andAbility->oneShot)
        {
            andAbilityClone->resolve();
            SAFE_DELETE(andAbilityClone);
        }
        else
        {
            andAbilityClone->addToGame();
        }
    }
    return 1;
}

int AManaProducer::reactToClick(MTGCardInstance * _card)
{
    if (!isReactingToClick(_card))
        return 0;
    if(!ActivatedAbility::isReactingToClick(_card))
        return 0;

    ManaCost * cost = getCost();
    if (cost)
    {
        cost->setExtraCostsAction(this, _card);
        if (!cost->isExtraPaymentSet())
        {
            game->mExtraPayment = cost->extraCosts;
            return 0;
        }
        if(cost->extraCosts){ // Added to check if the snow mana amount is enough to pay all the snow cost.
            int countSnow = 0;
            for(unsigned int i = 0; i < cost->extraCosts->costs.size(); i++){
                if(dynamic_cast<SnowCost*> (cost->extraCosts->costs[i]))
                    countSnow++;
            }
            if((source->controller()->snowManaG + source->controller()->snowManaU + source->controller()->snowManaR + 
                source->controller()->snowManaB + source->controller()->snowManaW + source->controller()->snowManaC) < countSnow){
                game->mExtraPayment = cost->extraCosts;
                return 0;
            }
        }
        cost->doPayExtra(); // Bring here brefore the normal payment to solve Snow Mana payment bug.
        game->currentlyActing()->getManaPool()->pay(cost);
    }

    if (options[Options::SFXVOLUME].number > 0)
    {
        WResourceManager::Instance()->PlaySample("mana.wav");
    }
    if(Producing.size())
        if(Producing.find("manapool") != string::npos)//unique card doubling cube.
            output->copy(source->controller()->getManaPool());
    return ActivatedAbility::activateAbility();
}

const string AManaProducer::getMenuText()
{
    if (menutext.size())
        return menutext.c_str();
    menutext = _("Add ");
    char buffer[128];
    int alreadyHasOne = 0;
    for (int i = 0; i < 8; i++)
    {
        int value = output->getCost(i);
        if (value)
        {
            if (alreadyHasOne)
                menutext.append(",");
            sprintf(buffer, "%i ", value);
            menutext.append(buffer);
            if (i == Constants::MTG_COLOR_WASTE)
                menutext.append(_(" colorless"));
            else if (i >= Constants::MTG_COLOR_GREEN && i <= Constants::MTG_COLOR_WASTE)
                menutext.append(_(Constants::MTGColorStrings[i]));
            
            alreadyHasOne = 1;
        }
    }
    menutext.append(_(" mana"));
    return menutext.c_str();
}

AManaProducer::~AManaProducer()
{
    SAFE_DELETE(output);
}

AManaProducer * AManaProducer::clone() const
{
    AManaProducer * a = NEW AManaProducer(*this);
    a->output = NEW ManaCost();
    a->output->copy(output);
    return a;
}


AbilityTP::AbilityTP(GameObserver* observer, int id, MTGCardInstance * card, Targetable * _target, int who) :
    MTGAbility(observer, id, card), who(who)
{
    if (_target)
        target = _target;
}

Targetable * AbilityTP::getTarget()
{
    switch (who)
    {
    case TargetChooser::TARGET_CONTROLLER:
        return getPlayerFromTarget(target);
    case TargetChooser::CONTROLLER:
        return source->controller();
    case TargetChooser::OPPONENT:
        return source->controller()->opponent();
    case TargetChooser::OWNER:
        return source->owner;
    case TargetChooser::TARGETED_PLAYER:
        return source->playerTarget;
    default:
        return target;
    }
    return NULL;
}

ActivatedAbilityTP::ActivatedAbilityTP(GameObserver* observer, int id, MTGCardInstance * card, Targetable * _target, ManaCost * cost, int who) :
    ActivatedAbility(observer, id, card, cost, 0), who(who)
{
    if (_target)
        target = _target;
}

Targetable * ActivatedAbilityTP::getTarget()
{
    switch (who)
    {
    case TargetChooser::TARGET_CONTROLLER:
        return getPlayerFromTarget(target);
    case TargetChooser::CONTROLLER:
        return source->controller();
    case TargetChooser::OPPONENT:
        return source->controller()->opponent();
    case TargetChooser::OWNER:
        return source->owner;
    case TargetChooser::TARGETED_PLAYER:
        return source->playerTarget;
    default:
        return target;
    }
    return NULL;
}

InstantAbilityTP::InstantAbilityTP(GameObserver* observer, int id, MTGCardInstance * card, Targetable * _target,int who) :
    InstantAbility(observer, id, card), who(who)
{
    if (_target)
        target = _target;    
}

//This is the same as Targetable * ActivatedAbilityTP::getTarget(), anyway to move them together?
Targetable * InstantAbilityTP::getTarget()
{
    switch (who)
    {
    case TargetChooser::TARGET_CONTROLLER:
        return getPlayerFromTarget(target);
    case TargetChooser::CONTROLLER:
        return source->controller();
    case TargetChooser::OPPONENT:
        return source->controller()->opponent();
    case TargetChooser::OWNER:
        return source->owner;
    default:
        return target;
    }
    return NULL;
}
