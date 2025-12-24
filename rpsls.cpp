//g++ -std=c++17 -o rpsls_game rpsls_game.cpp

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <random>
#include <algorithm>
#include <functional>
#include <limits>
#include <set>


enum class Choice {
    ROCK,
    SCISSORS,
    PAPER,
    LIZARD,
    SPOCK
};

class ChoiceHelper {
public:
    static std::string toString(Choice choice) {
        static const std::map<Choice, std::string> names = {
            {Choice::ROCK, "Камень"},
            {Choice::SCISSORS, "Ножницы"},
            {Choice::PAPER, "Бумага"},
            {Choice::LIZARD, "Ящерица"},
            {Choice::SPOCK, "Спок"}
        };
        return names.at(choice);
    }
    
    static Choice fromInput(const std::string& input) {
        static const std::map<std::string, Choice> mapping = {
            {"1", Choice::ROCK},
            {"2", Choice::SCISSORS},
            {"3", Choice::PAPER},
            {"4", Choice::LIZARD},
            {"5", Choice::SPOCK}
        };
        
        auto it = mapping.find(input);
        if (it != mapping.end()) {
            return it->second;
        }
        throw std::invalid_argument("Invalid input");
    }
    
    static std::vector<Choice> allChoices() {
        return {Choice::ROCK, Choice::SCISSORS, Choice::PAPER, 
                Choice::LIZARD, Choice::SPOCK};
    }
};


enum class DuelResult {
    WIN,
    LOSE,
    DRAW
};

class GameRules {
private:
    static const std::map<Choice, std::map<Choice, std::string>>& getWinsAgainst() {
        static const std::map<Choice, std::map<Choice, std::string>> wins = {
            {Choice::SCISSORS, {
                {Choice::PAPER, "Ножницы режут бумагу"},
                {Choice::LIZARD, "Ножницы обезглавливают ящерицу"}
            }},
            {Choice::PAPER, {
                {Choice::ROCK, "Бумага покрывает камень"},
                {Choice::SPOCK, "На бумаге улики против Спока"}
            }},
            {Choice::ROCK, {
                {Choice::LIZARD, "Камень давит ящерицу"},
                {Choice::SCISSORS, "Камень разбивает ножницы"}
            }},
            {Choice::LIZARD, {
                {Choice::SPOCK, "Ящерица отравляет Спока"},
                {Choice::PAPER, "Ящерица съедает бумагу"}
            }},
            {Choice::SPOCK, {
                {Choice::SCISSORS, "Спок ломает ножницы"},
                {Choice::ROCK, "Спок испаряет камень"}
            }}
        };
        return wins;
    }

public:
    static DuelResult compare(Choice choice1, Choice choice2) {
        if (choice1 == choice2) {
            return DuelResult::DRAW;
        }
        
        const auto& wins = getWinsAgainst();
        auto it1 = wins.find(choice1);
        if (it1 != wins.end()) {
            auto it2 = it1->second.find(choice2);
            if (it2 != it1->second.end()) {
                return DuelResult::WIN;
            }
        }
        
        return DuelResult::LOSE;
    }
    
    static std::string getDescription(Choice winner, Choice loser) {
        const auto& wins = getWinsAgainst();
        return wins.at(winner).at(loser);
    }
};


class ChoiceStrategy {
public:
    virtual ~ChoiceStrategy() = default;
    virtual Choice makeChoice(const std::vector<Choice>& history) = 0;
    virtual std::string getName() const = 0;
};

class RandomStrategy : public ChoiceStrategy {
private:
    std::mt19937 rng;
    
public:
    RandomStrategy() : rng(std::random_device{}()) {}
    
    Choice makeChoice(const std::vector<Choice>&) override {
        auto choices = ChoiceHelper::allChoices();
        std::uniform_int_distribution<size_t> dist(0, choices.size() - 1);
        return choices[dist(rng)];
    }
    
    std::string getName() const override {
        return "Случайная";
    }
};

class BiasedStrategy : public ChoiceStrategy {
private:
    std::mt19937 rng;
    std::map<Choice, int> weights;
    
public:
    BiasedStrategy() : rng(std::random_device{}()) {
        weights = {
            {Choice::ROCK, 2},
            {Choice::PAPER, 2},
            {Choice::SCISSORS, 2},
            {Choice::LIZARD, 1},
            {Choice::SPOCK, 1}
        };
    }
    
    Choice makeChoice(const std::vector<Choice>&) override {
        std::vector<Choice> pool;
        for (const auto& [choice, weight] : weights) {
            for (int i = 0; i < weight; ++i) {
                pool.push_back(choice);
            }
        }
        std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
        return pool[dist(rng)];
    }
    
    std::string getName() const override {
        return "Взвешенная";
    }
};

class AdaptiveStrategy : public ChoiceStrategy {
private:
    std::mt19937 rng;
    
    Choice findCounter(Choice target) {
        static const std::map<Choice, std::vector<Choice>> counters = {
            {Choice::ROCK, {Choice::PAPER, Choice::SPOCK}},
            {Choice::SCISSORS, {Choice::ROCK, Choice::SPOCK}},
            {Choice::PAPER, {Choice::SCISSORS, Choice::LIZARD}},
            {Choice::LIZARD, {Choice::ROCK, Choice::SCISSORS}},
            {Choice::SPOCK, {Choice::LIZARD, Choice::PAPER}}
        };
        
        const auto& options = counters.at(target);
        std::uniform_int_distribution<size_t> dist(0, options.size() - 1);
        return options[dist(rng)];
    }
    
public:
    AdaptiveStrategy() : rng(std::random_device{}()) {}
    
    Choice makeChoice(const std::vector<Choice>& history) override {
        if (history.size() < 3) {
            auto choices = ChoiceHelper::allChoices();
            std::uniform_int_distribution<size_t> dist(0, choices.size() - 1);
            return choices[dist(rng)];
        }
        
        std::map<Choice, int> counts;
        for (const auto& c : history) {
            counts[c]++;
        }
        
        Choice mostCommon = history[0];
        int maxCount = 0;
        for (const auto& [choice, count] : counts) {
            if (count > maxCount) {
                maxCount = count;
                mostCommon = choice;
            }
        }
        
        return findCounter(mostCommon);
    }
    
    std::string getName() const override {
        return "Адаптивная";
    }
};

class CyclicStrategy : public ChoiceStrategy {
private:
    size_t index = 0;
    std::vector<Choice> cycle;
    
public:
    CyclicStrategy() : cycle(ChoiceHelper::allChoices()) {}
    
    Choice makeChoice(const std::vector<Choice>&) override {
        Choice choice = cycle[index % cycle.size()];
        index++;
        return choice;
    }
    
    std::string getName() const override {
        return "Циклическая";
    }
};

class Player {
protected:
    std::string name_;
    std::vector<Choice> choiceHistory_;
    bool isActive_ = true;
    
public:
    explicit Player(const std::string& name) : name_(name) {}
    virtual ~Player() = default;
    
    const std::string& getName() const { return name_; }
    bool isActive() const { return isActive_; }
    void setActive(bool active) { isActive_ = active; }
    const std::vector<Choice>& getChoiceHistory() const { return choiceHistory_; }
    
    void recordChoice(Choice choice) {
        choiceHistory_.push_back(choice);
    }
    
    virtual Choice makeChoice() = 0;
    virtual std::string getType() const = 0;
    virtual bool isHuman() const = 0;
};

class HumanPlayer : public Player {
public:
    explicit HumanPlayer(const std::string& name) : Player(name) {}
    
    Choice makeChoice() override {
        std::cout << "\n  " << name_ << ", сделайте выбор:\n";
        std::cout << "    1. Камень\n";
        std::cout << "    2. Ножницы\n";
        std::cout << "    3. Бумага\n";
        std::cout << "    4. Ящерица\n";
        std::cout << "    5. Спок\n";
        
        while (true) {
            std::cout << "  Ваш выбор (1-5): ";
            std::string input;
            std::getline(std::cin, input);
            
            input.erase(0, input.find_first_not_of(" \t"));
            input.erase(input.find_last_not_of(" \t") + 1);
            
            try {
                Choice choice = ChoiceHelper::fromInput(input);
                recordChoice(choice);
                return choice;
            } catch (const std::invalid_argument&) {
                std::cout << "  Неверный ввод. Попробуйте снова.\n";
            }
        }
    }
    
    std::string getType() const override {
        return "Человек";
    }
    
    bool isHuman() const override { return true; }
};

class ComputerPlayer : public Player {
private:
    std::unique_ptr<ChoiceStrategy> strategy_;
    
public:
    ComputerPlayer(const std::string& name, std::unique_ptr<ChoiceStrategy> strategy)
        : Player(name), strategy_(std::move(strategy)) {}
    
    Choice makeChoice() override {
        Choice choice = strategy_->makeChoice(choiceHistory_);
        recordChoice(choice);
        return choice;
    }
    
    std::string getType() const override {
        return "Компьютер (" + strategy_->getName() + ")";
    }
    
    bool isHuman() const override { return false; }
};

class PlayerFactory {
private:
    static int humanCounter_;
    static int computerCounter_;
    static std::mt19937 rng_;
    
public:
    static void resetCounters() {
        humanCounter_ = 0;
        computerCounter_ = 0;
    }
    
    static std::unique_ptr<Player> createHuman(const std::string& name = "") {
        humanCounter_++;
        std::string playerName = name.empty() 
            ? "Игрок " + std::to_string(humanCounter_) 
            : name;
        return std::make_unique<HumanPlayer>(playerName);
    }
    
    static std::unique_ptr<Player> createComputer(const std::string& name = "") {
        computerCounter_++;
        std::string botName = name.empty() 
            ? "Бот " + std::to_string(computerCounter_) 
            : name;
        
        std::uniform_int_distribution<int> dist(0, 3);
        std::unique_ptr<ChoiceStrategy> strategy;
        
        switch (dist(rng_)) {
            case 0: strategy = std::make_unique<RandomStrategy>(); break;
            case 1: strategy = std::make_unique<BiasedStrategy>(); break;
            case 2: strategy = std::make_unique<AdaptiveStrategy>(); break;
            case 3: strategy = std::make_unique<CyclicStrategy>(); break;
            default: strategy = std::make_unique<RandomStrategy>(); break;
        }
        
        return std::make_unique<ComputerPlayer>(botName, std::move(strategy));
    }
    
    static std::vector<std::unique_ptr<Player>> createPlayers(int numHumans, int numComputers) {
        resetCounters();
        std::vector<std::unique_ptr<Player>> players;
        
        for (int i = 0; i < numHumans; ++i) {
            std::cout << "  Введите имя игрока " << (i + 1) << ": ";
            std::string name;
            std::getline(std::cin, name);
            
            name.erase(0, name.find_first_not_of(" \t"));
            if (!name.empty()) {
                name.erase(name.find_last_not_of(" \t") + 1);
            }
            
            players.push_back(createHuman(name));
        }
        
        for (int i = 0; i < numComputers; ++i) {
            players.push_back(createComputer());
        }
        
        return players;
    }
};

int PlayerFactory::humanCounter_ = 0;
int PlayerFactory::computerCounter_ = 0;
std::mt19937 PlayerFactory::rng_(std::random_device{}());


struct PlayerScore {
    Player* player;
    Choice choice;
    int wins = 0;
    int losses = 0;
    
    int getNetScore() const { return wins - losses; }
};


class RoundManager {
protected:
    std::mt19937 rng_;
    
public:
    RoundManager() : rng_(std::random_device{}()) {}
    virtual ~RoundManager() = default;
    
    // Возвращает список проигравших (пустой = ничья, нужна переигровка)
    std::vector<Player*> executeRound(std::vector<Player*>& players, 
                                       const std::string& groupName = "") {
        auto choices = collectChoices(players, groupName);
        
        printChoices(choices, groupName);
        
        auto scores = calculateScores(choices);
        
        printAllComparisons(choices, groupName);
        
        printScoreTable(scores, groupName);
        
        return determineLosers(scores, groupName);
    }
    
protected:
    std::vector<std::pair<Player*, Choice>> collectChoices(
            std::vector<Player*>& players, const std::string& groupName) {
        
        if (!groupName.empty()) {
            std::cout << "\n  [" << groupName << "] Игроки делают выбор...\n";
        }
        
        std::vector<std::pair<Player*, Choice>> choices;
        
        // Сначала люди
        std::vector<std::pair<Player*, Choice>> humanChoices;
        for (auto* player : players) {
            if (player->isHuman()) {
                Choice choice = player->makeChoice();
                humanChoices.push_back({player, choice});
            }
        }
        
        // Затем компьютеры
        for (auto* player : players) {
            if (!player->isHuman()) {
                Choice choice = player->makeChoice();
                choices.push_back({player, choice});
            }
        }
        
        for (auto& hc : humanChoices) {
            choices.push_back(hc);
        }
        
        return choices;
    }
    
    std::vector<PlayerScore> calculateScores(
            const std::vector<std::pair<Player*, Choice>>& choices) {
        
        std::vector<PlayerScore> scores;
        for (const auto& [player, choice] : choices) {
            scores.push_back({player, choice, 0, 0});
        }
        
        for (size_t i = 0; i < scores.size(); ++i) {
            for (size_t j = i + 1; j < scores.size(); ++j) {
                DuelResult result = GameRules::compare(scores[i].choice, scores[j].choice);
                
                switch (result) {
                    case DuelResult::WIN:
                        scores[i].wins++;
                        scores[j].losses++;
                        break;
                    case DuelResult::LOSE:
                        scores[i].losses++;
                        scores[j].wins++;
                        break;
                    case DuelResult::DRAW:
                        break;
                }
            }
        }
        
        return scores;
    }
    
    void printChoices(const std::vector<std::pair<Player*, Choice>>& choices,
                      const std::string& groupName) {
        if (!groupName.empty()) {
            std::cout << "\n  [" << groupName << "] Выборы игроков:\n";
        } else {
            std::cout << "\n  Выборы игроков:\n";
        }
        for (const auto& [player, choice] : choices) {
            std::cout << "    " << player->getName() << ": " 
                      << ChoiceHelper::toString(choice) << "\n";
        }
    }
    
    void printAllComparisons(const std::vector<std::pair<Player*, Choice>>& choices,
                             const std::string& groupName) {
        if (!groupName.empty()) {
            std::cout << "\n  [" << groupName << "] Сравнения:\n";
        } else {
            std::cout << "\n  Сравнения:\n";
        }
        
        for (size_t i = 0; i < choices.size(); ++i) {
            for (size_t j = i + 1; j < choices.size(); ++j) {
                const auto& [player1, choice1] = choices[i];
                const auto& [player2, choice2] = choices[j];
                
                DuelResult result = GameRules::compare(choice1, choice2);
                
                if (result == DuelResult::DRAW) {
                    std::cout << "    " << player1->getName() << " = " 
                              << player2->getName() << " (ничья)\n";
                } else if (result == DuelResult::WIN) {
                    std::cout << "    " << player1->getName() << " > " 
                              << player2->getName() << " -- "
                              << GameRules::getDescription(choice1, choice2) << "\n";
                } else {
                    std::cout << "    " << player2->getName() << " > " 
                              << player1->getName() << " -- "
                              << GameRules::getDescription(choice2, choice1) << "\n";
                }
            }
        }
    }
    
    void printScoreTable(const std::vector<PlayerScore>& scores,
                         const std::string& groupName) {
        if (!groupName.empty()) {
            std::cout << "\n  [" << groupName << "] Итоги:\n";
        } else {
            std::cout << "\n  Итоги раунда:\n";
        }

        auto sortedScores = scores;
        std::sort(sortedScores.begin(), sortedScores.end(),
            [](const PlayerScore& a, const PlayerScore& b) {
                if (a.getNetScore() != b.getNetScore()) {
                    return a.getNetScore() > b.getNetScore();
                }
                return a.wins > b.wins;
            });
        
        for (const auto& score : sortedScores) {
            std::string balanceStr;
            int net = score.getNetScore();
            if (net > 0) balanceStr = "+" + std::to_string(net);
            else balanceStr = std::to_string(net);
            
            std::cout << "    " << score.player->getName() 
                      << " -- " << score.wins << "W/" << score.losses << "L"
                      << " (баланс: " << balanceStr << ")\n";
        }
    }
    
    std::vector<Player*> determineLosers(std::vector<PlayerScore>& scores,
                                          const std::string& groupName) {
        // Находим минимальный и максимальный баланс
        int minNetScore = scores[0].getNetScore();
        int maxNetScore = scores[0].getNetScore();
        for (const auto& score : scores) {
            if (score.getNetScore() < minNetScore) {
                minNetScore = score.getNetScore();
            }
            if (score.getNetScore() > maxNetScore) {
                maxNetScore = score.getNetScore();
            }
        }
        
        // Если у всех одинаковый баланс - ничья, переигровка
        if (minNetScore == maxNetScore) {
            if (!groupName.empty()) {
                std::cout << "\n  [" << groupName << "] Ничья! Переигровка...\n";
            } else {
                std::cout << "\n  Ничья! Переигровка...\n";
            }
            return {}; // Пустой список = переигровка
        }
        
        // Собираем проигравших
        std::vector<Player*> losers;
        for (const auto& score : scores) {
            if (score.getNetScore() == minNetScore) {
                losers.push_back(score.player);
                if (!groupName.empty()) {
                    std::cout << "\n  [" << groupName << "] " << score.player->getName() 
                              << " выбывает! (" << score.wins << "W/" << score.losses << "L)\n";
                } else {
                    std::cout << "\n  " << score.player->getName() 
                              << " выбывает! (" << score.wins << "W/" << score.losses << "L)\n";
                }
            }
        }
        
        return losers;
    }
};


// Класс для разделения на группы
class GroupDivider {
private:
    std::mt19937 rng_;
    
public:
    GroupDivider() : rng_(std::random_device{}()) {}
    
    // Разделяет игроков на группы по 2-4 человека
    // Ни один игрок не должен остаться без группы
    std::vector<std::vector<Player*>> divideIntoGroups(std::vector<Player*>& players) {
        std::vector<std::vector<Player*>> groups;
        
        // Перемешиваем игроков
        std::vector<Player*> shuffled = players;
        std::shuffle(shuffled.begin(), shuffled.end(), rng_);
        
        int n = static_cast<int>(shuffled.size());
        
        // Алгоритм разбиения:
        // - Стараемся набрать группы по 4
        // - Если остаток 1, то заменяем одну четвёрку на 3+2
        // - Если остаток 2, добавляем группу из 2
        // - Если остаток 3, добавляем группу из 3
        
        int numFours = n / 4;
        int remainder = n % 4;
        
        std::vector<int> groupSizes;
        
        if (remainder == 0) {
            // Все группы по 4
            for (int i = 0; i < numFours; ++i) {
                groupSizes.push_back(4);
            }
        } else if (remainder == 1) {
            // (numFours-1) групп по 4, затем 3 и 2
            for (int i = 0; i < numFours - 1; ++i) {
                groupSizes.push_back(4);
            }
            groupSizes.push_back(3);
            groupSizes.push_back(2);
        } else if (remainder == 2) {
            // numFours групп по 4, затем 2
            for (int i = 0; i < numFours; ++i) {
                groupSizes.push_back(4);
            }
            groupSizes.push_back(2);
        } else { // remainder == 3
            // numFours групп по 4, затем 3
            for (int i = 0; i < numFours; ++i) {
                groupSizes.push_back(4);
            }
            groupSizes.push_back(3);
        }
        
        // Создаём группы
        size_t idx = 0;
        for (int size : groupSizes) {
            std::vector<Player*> group;
            for (int i = 0; i < size && idx < shuffled.size(); ++i) {
                group.push_back(shuffled[idx++]);
            }
            groups.push_back(group);
        }
        
        return groups;
    }
};


class Game {
private:
    std::vector<std::unique_ptr<Player>> players_;
    std::unique_ptr<RoundManager> roundManager_;
    std::unique_ptr<GroupDivider> groupDivider_;
    int roundNumber_ = 0;
    
    std::vector<Player*> getActivePlayers() {
        std::vector<Player*> active;
        for (auto& player : players_) {
            if (player->isActive()) {
                active.push_back(player.get());
            }
        }
        return active;
    }
    
    int readInt(const std::string& prompt) {
        while (true) {
            std::cout << prompt << std::flush;
            std::string input;
            std::getline(std::cin, input);
            
            try {
                return std::stoi(input);
            } catch (...) {
                std::cout << "  Введите целое число!\n";
            }
        }
    }
    
    // Проводит раунд в одной группе с переигровками до победителя
    void playGroupRound(std::vector<Player*>& group, const std::string& groupName) {
        while (true) {
            std::vector<Player*> losers = roundManager_->executeRound(group, groupName);
            
            if (losers.empty()) {
                // Ничья - переигровка
                continue;
            }
            
            // Деактивируем проигравших
            for (auto* loser : losers) {
                loser->setActive(false);
            }
            
            // Убираем проигравших из группы
            group.erase(
                std::remove_if(group.begin(), group.end(),
                    [](Player* p) { return !p->isActive(); }),
                group.end()
            );
            
            break; // Раунд завершён
        }
    }
    
    // Проводит раунд для всех игроков (с разделением на группы если нужно)
    void playRound(std::vector<Player*>& activePlayers) {
        if (activePlayers.size() > 5) {
            // Разделяем на группы
            auto groups = groupDivider_->divideIntoGroups(activePlayers);
            
            std::cout << "\n  Игроков много (" << activePlayers.size() 
                      << "), разделяем на " << groups.size() << " групп(ы):\n";
            
            for (size_t i = 0; i < groups.size(); ++i) {
                std::cout << "    Группа " << (i + 1) << ": ";
                for (size_t j = 0; j < groups[i].size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << groups[i][j]->getName();
                }
                std::cout << "\n";
            }
            
            // Проводим раунд в каждой группе
            for (size_t i = 0; i < groups.size(); ++i) {
                std::string groupName = "Группа " + std::to_string(i + 1);
                std::cout << "\n" << std::string(40, '-') << "\n";
                playGroupRound(groups[i], groupName);
            }
            
        } else {
            // Играем все вместе с переигровками
            while (true) {
                std::vector<Player*> losers = roundManager_->executeRound(activePlayers);
                
                if (losers.empty()) {
                    // Ничья - переигровка
                    continue;
                }
                
                // Деактивируем проигравших
                for (auto* loser : losers) {
                    loser->setActive(false);
                }
                
                break;
            }
        }
    }
    
public:
    Game() : roundManager_(std::make_unique<RoundManager>()),
             groupDivider_(std::make_unique<GroupDivider>()) {}
    
    void setup() {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "  КАМЕНЬ-НОЖНИЦЫ-БУМАГА-ЯЩЕРИЦА-СПОК\n";
        std::cout << "  Режим: Все против всех\n";
        std::cout << "  Ave Deus Mechanicus!\n";
        std::cout << std::string(60, '=') << "\n";
        
        std::cout << "\n  Правила:\n";
        std::cout << "  - Ножницы режут бумагу, бумага покрывает камень\n";
        std::cout << "  - Камень давит ящерицу, ящерица отравляет Спока\n";
        std::cout << "  - Спок ломает ножницы, ножницы обезглавливают ящерицу\n";
        std::cout << "  - Ящерица съедает бумагу, на бумаге улики против Спока\n";
        std::cout << "  - Спок испаряет камень, камень разбивает ножницы\n";
        std::cout << "\n  Механика:\n";
        std::cout << "  - Каждый раунд все делают выбор одновременно\n";
        std::cout << "  - Игрок(и) с худшим балансом побед/поражений выбывают\n";
        std::cout << "  - При ничьей - переигровка\n";
        std::cout << "  - Если игроков > 5, они делятся на группы по 2-4\n";
        std::cout << "  - Последний оставшийся - победитель!\n" << std::flush;
        
        int numHumans, numComputers;
        
        while (true) {
            numHumans = readInt("\n  Количество игроков-людей: ");
            if (numHumans < 0) {
                std::cout << "  Число не может быть отрицательным!\n";
                continue;
            }
            break;
        }
        
        while (true) {
            numComputers = readInt("  Количество игроков-компьютеров: ");
            if (numComputers < 0) {
                std::cout << "  Число не может быть отрицательным!\n";
                continue;
            }
            
            int total = numHumans + numComputers;
            if (total < 2) {
                std::cout << "  Для игры нужно минимум 2 участника!\n";
                continue;
            }
            
            if (numHumans == 1 && numComputers == 0) {
                std::cout << "  Один игрок не может играть сам с собой,\n";
                std::cout << "  Добавьте хотя бы одного компьютера.\n";
                continue;
            }
            
            break;
        }
        
        std::cout << "\n";
        players_ = PlayerFactory::createPlayers(numHumans, numComputers);
        
        std::cout << "\n  Участники турнира:\n";
        int i = 1;
        for (const auto& player : players_) {
            std::cout << "    " << i++ << ". " << player->getName() 
                      << " (" << player->getType() << ")\n";
        }
    }
    
    void run() {
        while (getActivePlayers().size() > 1) {
            roundNumber_++;
            auto activePlayers = getActivePlayers();
            
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "  РАУНД " << roundNumber_ << "\n";
            std::cout << "  Осталось игроков: " << activePlayers.size() << "\n";
            std::cout << std::string(60, '=') << "\n";
            
            playRound(activePlayers);
            
            if (getActivePlayers().size() > 1) {
                std::cout << "\n  Нажмите Enter для продолжения...";
                std::cin.get();
            }
        }
        
        auto finalPlayers = getActivePlayers();
        if (!finalPlayers.empty()) {
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "  ПОБЕДИТЕЛЬ: " << finalPlayers[0]->getName() << "\n";
            std::cout << std::string(60, '=') << "\n";
        } else {
            std::cout << "\n  Все игроки выбыли одновременно, ничья\n";
        }
    }
};


int main() {
    Game game;
    
    try {
        game.setup();
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "\n  Ошибка: " << e.what() << "\n";
    }
    
    std::cout << "\n  Наигрались, закругляемся.\n";
    
    return 0;
}
