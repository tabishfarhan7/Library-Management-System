#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>

#include "httplib.h"
#include "json.hpp"
using namespace std;


using json = nlohmann::json;


// Forward declarations
class Book;
class User;

// Abstract base class for library items
class LibraryItem {
protected:
    string title;
    string id;
    bool available;

public:
    LibraryItem(const string& title, const string& id) 
        : title(title), id(id), available(true) {}

    virtual ~LibraryItem() {}

    string getTitle() const { return title; }
    string getId() const { return id; }
    bool isAvailable() const { return available; }
    void setAvailable(bool status) { available = status; }

    virtual void displayDetails() const = 0;
};

// Book class inheriting from LibraryItem
class Book : public LibraryItem {
    string author;
    string isbn;
    string genre;
    int publicationYear;

public:
    Book(const string& title, const string& author, const string& isbn, 
         const string& genre, int publicationYear)
        : LibraryItem(title, isbn), author(author), isbn(isbn), 
          genre(genre), publicationYear(publicationYear) {}

    string getAuthor() const { return author; }
    string getIsbn() const { return isbn; }
    string getGenre() const { return genre; }
    int getPublicationYear() const { return publicationYear; }

    void displayDetails() const override {
        cout << "Title: " << title << "\n"
             << "Author: " << author << "\n"
             << "ISBN: " << isbn << "\n"
             << "Genre: " << genre << "\n"
             << "Publication Year: " << publicationYear << "\n"
             << "Available: " << (available ? "Yes" : "No") << "\n";
    }
};

// User class
class User {
    string userId;
    string name;
    string email;
    vector<pair<Book*, time_t>> borrowedBooks; // Book pointer and due date

public:
    User(const string& userId, const string& name, const string& email)
        : userId(userId), name(name), email(email) {}

    string getUserId() const { return userId; }
    string getName() const { return name; }
    string getEmail() const { return email; }
    const vector<pair<Book*, time_t>>& getBorrowedBooks() const { return borrowedBooks; }

    bool borrowBook(Book* book) {
        if (book && book->isAvailable()) {
            book->setAvailable(false);
            time_t now = time(nullptr);
            time_t dueDate = now + 14 * 24 * 60 * 60; // 14 days in seconds
            borrowedBooks.emplace_back(book, dueDate);
            return true;
        }
        return false;
    }

    bool returnBook(Book* book) {
        for (auto it = borrowedBooks.begin(); it != borrowedBooks.end(); ++it) {
            if (it->first == book) {
                book->setAvailable(true);
                borrowedBooks.erase(it);
                return true;
            }
        }
        return false;
    }

    void displayDetails() const {
        cout << "User ID: " << userId << "\n"
             << "Name: " << name << "\n"
             << "Email: " << email << "\n"
             << "Books Borrowed: " << borrowedBooks.size() << "\n";
    }

    void displayBorrowedBooks() const {
        if (borrowedBooks.empty()) {
            cout << "No books currently borrowed.\n";
            return;
        }

        cout << "Borrowed Books:\n";
        for (const auto& entry : borrowedBooks) {
            time_t dueDate = entry.second;
            tm* tmDueDate = localtime(&dueDate);
            cout << "- " << entry.first->getTitle() 
                 << " (Due: " << put_time(tmDueDate, "%Y-%m-%d") << ")\n";
        }
    }
};

// Library class (Singleton)
class Library {

    httplib::Server svr;  // Add this as a private member variable
    bool serverRunning = false;
    static Library* instance;
    vector<Book*> books;
    vector<User*> users;

    // Private constructor for singleton
    Library() {
        loadData();
    }

public:

     void startServer() {
        if (serverRunning) return;
        
        // Enable CORS
        svr.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"}
        });

        // Get all books
        svr.Get("/api/books", [this](const auto& req, auto& res) {
            json books_json;
            for (auto book : books) {
                books_json.push_back({
                    {"title", book->getTitle()},
                    {"author", book->getAuthor()},
                    {"isbn", book->getIsbn()},
                    {"genre", book->getGenre()},
                    {"year", book->getPublicationYear()},
                    {"available", book->isAvailable()}
                });
            }
            res.set_content(books_json.dump(), "application/json");
        });

        // Search books
        svr.Get("/api/books/search", [this](const auto& req, auto& res) {
            string query = req.get_param_value("q");
            vector<Book*> results;
            
            // Search in title, author, and genre
            for (auto book : books) {
                if (book->getTitle().find(query) != string::npos ||
                    book->getAuthor().find(query) != string::npos ||
                    book->getGenre().find(query) != string::npos) {
                    results.push_back(book);
                }
            }
            
            json response;
            for (auto book : results) {
                response.push_back({
                    {"title", book->getTitle()},
                    {"author", book->getAuthor()},
                    {"isbn", book->getIsbn()},
                    {"available", book->isAvailable()}
                });
            }
            res.set_content(response.dump(), "application/json");
        });

        // User login
        svr.Post("/api/login", [this](const auto& req, auto& res) {
            auto body = json::parse(req.body);
            string email = body["email"];
            User* user = findUserByEmail(email);
            
            if (user) {
                res.set_content(json{
                    {"success", true},
                    {"name", user->getName()},
                    {"email", user->getEmail()},
                    {"userId", user->getUserId()}
                }.dump(), "application/json");
            } else {
                res.set_content(json{{"success", false}}.dump(), "application/json");
            }
        });

        serverRunning = true;
        std::thread server_thread([this]() {
            std::cout << "Server running on http://localhost:8080\n";
            svr.listen("localhost", 8080);
        });
        server_thread.detach();
    }

    void stopServer() {
        if (serverRunning) {
            svr.stop();
            serverRunning = false;
        }
    }

    // Delete copy constructor and assignment operator
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    static Library* getInstance() {
        if (!instance) {
            instance = new Library();
        }
        return instance;
    }

    ~Library() {
        for (auto book : books) delete book;
        for (auto user : users) delete user;
    }

    void addBook(Book* book) {
        books.push_back(book);
        saveData();
    }

    void addUser(User* user) {
        users.push_back(user);
        saveData();
    }

    Book* findBookByTitle(const string& title) {
        // Linear search
        for (auto book : books) {
            if (book->getTitle() == title) {
                return book;
            }
        }
        return nullptr;
    }

    vector<Book*> findBooksByAuthor(const string& author) {
        vector<Book*> result;
        for (auto book : books) {
            if (book->getAuthor() == author) {
                result.push_back(book);
            }
        }
        return result;
    }

    vector<Book*> findBooksByGenre(const string& genre) {
        vector<Book*> result;
        // First sort books by genre for binary search
        sort(books.begin(), books.end(), [](Book* a, Book* b) {
            return a->getGenre() < b->getGenre();
        });

        // Binary search implementation
        auto low = lower_bound(books.begin(), books.end(), genre, 
            [](Book* book, const string& genre) {
                return book->getGenre() < genre;
            });

        auto high = upper_bound(books.begin(), books.end(), genre, 
            [](const string& genre, Book* book) {
                return genre < book->getGenre();
            });

        for (auto it = low; it != high; ++it) {
            result.push_back(*it);
        }

        return result;
    }

    User* findUserById(const string& userId) {
        for (auto user : users) {
            if (user->getUserId() == userId) {
                return user;
            }
        }
        return nullptr;
    }

    User* findUserByEmail(const string& email) {
        for (auto user : users) {
            if (user->getEmail() == email) {
                return user;
            }
        }
        return nullptr;
    }

    void displayAllBooks() {
        // Bubble sort by title
        for (size_t i = 0; i < books.size(); ++i) {
            for (size_t j = 0; j < books.size() - i - 1; ++j) {
                if (books[j]->getTitle() > books[j+1]->getTitle()) {
                    swap(books[j], books[j+1]);
                }
            }
        }

        cout << "\n===== All Books =====\n";
        for (auto book : books) {
            book->displayDetails();
            cout << "--------------------\n";
        }
    }

    void saveData() {
        ofstream outFile("library_data.txt");
        if (!outFile) {
            cerr << "Error saving data to file.\n";
            return;
        }

        // Save books
        outFile << "[BOOKS]\n";
        for (auto book : books) {
            outFile << book->getTitle() << ","
                   << book->getAuthor() << ","
                   << book->getIsbn() << ","
                   << book->getGenre() << ","
                   << book->getPublicationYear() << ","
                   << book->isAvailable() << "\n";
        }

        // Save users
        outFile << "[USERS]\n";
        for (auto user : users) {
            outFile << user->getUserId() << ","
                   << user->getName() << ","
                   << user->getEmail() << "\n";

            // Save borrowed books
            outFile << "[BORROWED]" << user->getUserId() << "\n";
            for (const auto& entry : user->getBorrowedBooks()) {
                outFile << entry.first->getIsbn() << ","
                       << entry.second << "\n";
            }
        }

        outFile.close();
    }

    void loadData() {
        ifstream inFile("library_data.txt");
        if (!inFile) {
            return; // No existing data file
        }

        string line;
        string section;
        User* currentUser = nullptr;

        while (getline(inFile, line)) {
            if (line.empty()) continue;

            if (line[0] == '[') {
                section = line;
                continue;
            }

            if (section == "[BOOKS]") {
                stringstream ss(line);
                string title, author, isbn, genre, availableStr;
                int publicationYear;
                bool available;

                getline(ss, title, ',');
                getline(ss, author, ',');
                getline(ss, isbn, ',');
                getline(ss, genre, ',');
                ss >> publicationYear;
                ss.ignore(); // Ignore comma
                ss >> available;

                Book* book = new Book(title, author, isbn, genre, publicationYear);
                book->setAvailable(available);
                books.push_back(book);
            }
            else if (section == "[USERS]") {
                stringstream ss(line);
                string userId, name, email;
                getline(ss, userId, ',');
                getline(ss, name, ',');
                getline(ss, email, ',');

                User* user = new User(userId, name, email);
                users.push_back(user);
                currentUser = user;
            }
            else if (section.find("[BORROWED]") != string::npos) {
                if (!currentUser) continue;

                stringstream ss(line);
                string isbn;
                time_t dueDate;
                getline(ss, isbn, ',');
                ss >> dueDate;

                Book* book = findBookByIsbn(isbn);
                if (book) {
                    currentUser->borrowBook(book);
                    // Set the due date (since borrowBook creates a new entry)
                    if (!currentUser->getBorrowedBooks().empty()) {
                        const_cast<pair<Book*, time_t>&>(currentUser->getBorrowedBooks().back()).second = dueDate;
                    }
                }
            }
        }

        inFile.close();
    }

    Book* findBookByIsbn(const string& isbn) {
        for (auto book : books) {
            if (book->getIsbn() == isbn) {
                return book;
            }
        }
        return nullptr;
    }

    const vector<Book*>& getAllBooks() const { return books; }
    const vector<User*>& getAllUsers() const { return users; }
};

// Initialize static member
Library* Library::instance = nullptr;

// Library Application class
class LibraryApp {
    Library* library;
    User* currentUser;

public:
    LibraryApp() : library(Library::getInstance()), currentUser(nullptr) {}

    void run() {
        while (true) {
            displayMenu();
            int choice = getChoice();

            if (currentUser) {
                switch (choice) {
                    case 1: browseBooks(); break;
                    case 2: searchBooks(); break;
                    case 3: borrowBook(); break;
                    case 4: returnBook(); break;
                    case 5: viewAccount(); break;
                    case 6: logout(); break;
                    case 7: return; // Exit
                    default: cout << "Invalid choice. Please try again.\n";
                }
            } else {
                switch (choice) {
                    case 1: browseBooks(); break;
                    case 2: searchBooks(); break;
                    case 3: registerUser(); break;
                    case 4: login(); break;
                    case 5: return; // Exit
                    default: cout << "Invalid choice. Please try again.\n";
                }
            }
        }
    }

private:
    void displayMenu() {
        cout << "\n===== Library Management System =====\n";
        if (currentUser) {
            cout << "Logged in as: " << currentUser->getName() << "\n";
            cout << "1. Browse Books\n";
            cout << "2. Search Books\n";
            cout << "3. Borrow a Book\n";
            cout << "4. Return a Book\n";
            cout << "5. View My Account\n";
            cout << "6. Logout\n";
            cout << "7. Exit\n";
        } else {
            cout << "1. Browse Books\n";
            cout << "2. Search Books\n";
            cout << "3. Register\n";
            cout << "4. Login\n";
            cout << "5. Exit\n";
        }
    }

    int getChoice() {
        int choice;
        cout << "Enter your choice: ";
        while (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input. Please enter a number: ";
        }
        cin.ignore(); // Clear newline character
        return choice;
    }

    void browseBooks() {
        library->displayAllBooks();
    }

    void searchBooks() {
        cout << "\n===== Search Books =====\n";
        cout << "1. Search by Title\n";
        cout << "2. Search by Author\n";
        cout << "3. Search by Genre\n";
        cout << "4. Back to Main Menu\n";

        int choice = getChoice();
        string query;

        switch (choice) {
            case 1:
                cout << "Enter book title: ";
                getline(cin, query);
                {
                    Book* book = library->findBookByTitle(query);
                    if (book) {
                        cout << "\nBook Found:\n";
                        book->displayDetails();
                    } else {
                        cout << "Book not found.\n";
                    }
                }
                break;
            case 2:
                cout << "Enter author name: ";
                getline(cin, query);
                {
                    vector<Book*> books = library->findBooksByAuthor(query);
                    if (!books.empty()) {
                        cout << "\nBooks by " << query << ":\n";
                        for (auto book : books) {
                            book->displayDetails();
                            cout << "--------------------\n";
                        }
                    } else {
                        cout << "No books found by this author.\n";
                    }
                }
                break;
            case 3:
                cout << "Enter genre: ";
                getline(cin, query);
                {
                    vector<Book*> books = library->findBooksByGenre(query);
                    if (!books.empty()) {
                        cout << "\nBooks in " << query << " genre:\n";
                        for (auto book : books) {
                            book->displayDetails();
                            cout << "--------------------\n";
                        }
                    } else {
                        cout << "No books found in this genre.\n";
                    }
                }
                break;
            case 4:
                return;
            default:
                cout << "Invalid choice.\n";
        }
    }

    void registerUser() {
        cout << "\n===== User Registration =====\n";
        string userId, name, email;

        cout << "Enter user ID: ";
        getline(cin, userId);
        if (library->findUserById(userId)) {
            cout << "User ID already exists.\n";
            return;
        }

        cout << "Enter your name: ";
        getline(cin, name);

        cout << "Enter your email: ";
        getline(cin, email);
        if (library->findUserByEmail(email)) {
            cout << "Email already registered.\n";
            return;
        }

        User* newUser = new User(userId, name, email);
        library->addUser(newUser);
        cout << "Registration successful! You can now login.\n";
    }

    void login() {
        cout << "\n===== User Login =====\n";
        string email;

        cout << "Enter your email: ";
        getline(cin, email);

        User* user = library->findUserByEmail(email);
        if (user) {
            cout << "Welcome back, " << user->getName() << "!\n";
            currentUser = user;
        } else {
            cout << "User not found. Please register first.\n";
        }
    }

    void logout() {
        currentUser = nullptr;
        cout << "You have been logged out.\n";
    }

    void borrowBook() {
        cout << "\n===== Borrow a Book =====\n";
        string title;

        cout << "Enter the title of the book you want to borrow: ";
        getline(cin, title);

        Book* book = library->findBookByTitle(title);
        if (!book) {
            cout << "Book not found.\n";
            return;
        }

        if (!book->isAvailable()) {
            cout << "This book is currently not available.\n";
            return;
        }

        if (currentUser->borrowBook(book)) {
            library->saveData();
            time_t dueDate = time(nullptr) + 14 * 24 * 60 * 60;
            tm* tmDueDate = localtime(&dueDate);
            cout << "You have successfully borrowed '" << book->getTitle() 
                 << "'. Due date: " << put_time(tmDueDate, "%Y-%m-%d") << "\n";
        } else {
            cout << "Failed to borrow the book.\n";
        }
    }

    void returnBook() {
        cout << "\n===== Return a Book =====\n";
        const auto& borrowedBooks = currentUser->getBorrowedBooks();
        if (borrowedBooks.empty()) {
            cout << "You have no books to return.\n";
            return;
        }

        cout << "Your borrowed books:\n";
        for (size_t i = 0; i < borrowedBooks.size(); ++i) {
            time_t dueDate = borrowedBooks[i].second;
            tm* tmDueDate = localtime(&dueDate);
            cout << i+1 << ". " << borrowedBooks[i].first->getTitle()
                 << " (Due: " << put_time(tmDueDate, "%Y-%m-%d") << ")\n";
        }

        cout << "Enter the number of the book you want to return: ";
        size_t choice;
        cin >> choice;
        cin.ignore();

        if (choice > 0 && choice <= borrowedBooks.size()) {
            Book* book = borrowedBooks[choice-1].first;
            if (currentUser->returnBook(book)) {
                library->saveData();
                cout << "You have successfully returned '" << book->getTitle() << "'.\n";
            } else {
                cout << "Failed to return the book.\n";
            }
        } else {
            cout << "Invalid selection.\n";
        }
    }

    void viewAccount() {
        cout << "\n===== My Account =====\n";
        currentUser->displayDetails();
        currentUser->displayBorrowedBooks();
    }
};


int main(int argc, char* argv[]) {
    // Check for --server argument
    bool runAsServer = (argc > 1 && std::string(argv[1]) == "--server");

    if (runAsServer) {
        Library::getInstance()->startServer();
        
        // Keep the server running
        std::cout << "Press Enter to stop the server...\n";
        std::cin.ignore();
        Library::getInstance()->stopServer();
    } else {
        // Run in normal console mode
        LibraryApp app;
        app.run();
    }

    return 0;
}