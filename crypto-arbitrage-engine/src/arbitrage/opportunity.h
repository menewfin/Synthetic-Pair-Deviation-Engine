#pragma once

#include "core/types.h"
#include <algorithm>
#include <functional>

namespace arbitrage {

// Opportunity filters
class OpportunityFilter {
public:
    virtual ~OpportunityFilter() = default;
    virtual bool accept(const ArbitrageOpportunity& opportunity) const = 0;
};

class MinProfitFilter : public OpportunityFilter {
public:
    explicit MinProfitFilter(double min_profit_bps) : min_profit_bps_(min_profit_bps) {}
    
    bool accept(const ArbitrageOpportunity& opportunity) const override {
        return opportunity.profit_percentage * 100 >= min_profit_bps_;
    }
    
private:
    double min_profit_bps_;
};

class MaxCapitalFilter : public OpportunityFilter {
public:
    explicit MaxCapitalFilter(double max_capital) : max_capital_(max_capital) {}
    
    bool accept(const ArbitrageOpportunity& opportunity) const override {
        return opportunity.required_capital <= max_capital_;
    }
    
private:
    double max_capital_;
};

class RiskFilter : public OpportunityFilter {
public:
    explicit RiskFilter(double max_risk) : max_risk_(max_risk) {}
    
    bool accept(const ArbitrageOpportunity& opportunity) const override {
        return opportunity.execution_risk <= max_risk_;
    }
    
private:
    double max_risk_;
};

// Opportunity ranking
class OpportunityRanker {
public:
    // Score function type
    using ScoreFunction = std::function<double(const ArbitrageOpportunity&)>;
    
    // Add scoring criteria
    void add_criteria(ScoreFunction func, double weight) {
        criteria_.push_back({func, weight});
    }
    
    // Calculate total score for an opportunity
    double score(const ArbitrageOpportunity& opportunity) const {
        double total_score = 0.0;
        double total_weight = 0.0;
        
        for (const auto& [func, weight] : criteria_) {
            total_score += func(opportunity) * weight;
            total_weight += weight;
        }
        
        return total_weight > 0 ? total_score / total_weight : 0.0;
    }
    
    // Rank opportunities
    std::vector<ArbitrageOpportunity> rank(std::vector<ArbitrageOpportunity> opportunities) const {
        std::sort(opportunities.begin(), opportunities.end(),
                 [this](const auto& a, const auto& b) {
                     return score(a) > score(b);
                 });
        return opportunities;
    }
    
    // Default scoring functions
    static double profit_score(const ArbitrageOpportunity& opp) {
        return std::min(opp.profit_percentage / 10.0, 1.0);  // Normalize to 0-1
    }
    
    static double risk_score(const ArbitrageOpportunity& opp) {
        return 1.0 - opp.execution_risk;  // Lower risk = higher score
    }
    
    static double liquidity_score(const ArbitrageOpportunity& opp) {
        return opp.liquidity_score;
    }
    
    static double capital_efficiency_score(const ArbitrageOpportunity& opp) {
        return opp.expected_profit / opp.required_capital;
    }
    
private:
    std::vector<std::pair<ScoreFunction, double>> criteria_;
};

// Opportunity aggregator
class OpportunityAggregator {
public:
    void add_opportunity(const ArbitrageOpportunity& opportunity) {
        opportunities_.push_back(opportunity);
    }
    
    std::vector<ArbitrageOpportunity> get_filtered(
        const std::vector<std::unique_ptr<OpportunityFilter>>& filters) const {
        
        std::vector<ArbitrageOpportunity> filtered;
        
        for (const auto& opp : opportunities_) {
            bool accept = true;
            for (const auto& filter : filters) {
                if (!filter->accept(opp)) {
                    accept = false;
                    break;
                }
            }
            if (accept) {
                filtered.push_back(opp);
            }
        }
        
        return filtered;
    }
    
    void clear() {
        opportunities_.clear();
    }
    
    size_t size() const {
        return opportunities_.size();
    }
    
private:
    std::vector<ArbitrageOpportunity> opportunities_;
};

} // namespace arbitrage