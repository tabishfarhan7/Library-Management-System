// This would normally connect to a backend API
// For our C++ backend, we'll simulate the connection

document.addEventListener('DOMContentLoaded', () => {
    const loginSection = document.getElementById('login-section');
    const mainSection = document.getElementById('main-section');
    const loginBtn = document.getElementById('login-btn');
    const registerBtn = document.getElementById('register-btn');
    const searchBtn = document.getElementById('search-btn');
    const userStatus = document.getElementById('user-status');
    const emailInput = document.getElementById('email');
    
    let currentUser = null;

    // Mock data - in real app this would come from backend
    const mockBooks = [
        {
            title: "The Great Gatsby",
            author: "F. Scott Fitzgerald",
            isbn: "9780743273565",
            genre: "Classic",
            year: 1925,
            available: true
        },
        {
            title: "To Kill a Mockingbird",
            author: "Harper Lee",
            isbn: "9780061120084",
            genre: "Fiction",
            year: 1960,
            available: true
        }
    ];

    // Simulate backend connection
    class BackendService {
        static login(email) {
            // In real app: fetch from C++ backend
            return new Promise((resolve) => {
                setTimeout(() => {
                    resolve({
                        userId: "user123",
                        name: "Test User",
                        email: email,
                        borrowedBooks: []
                    });
                }, 500);
            });
        }

        static searchBooks(query) {
            // In real app: fetch from C++ backend
            return new Promise((resolve) => {
                setTimeout(() => {
                    resolve(mockBooks.filter(book => 
                        book.title.includes(query) || 
                        book.author.includes(query) ||
                        book.genre.includes(query)
                    ));
                }, 500);
            });
        }
    }

    // Event listeners
    loginBtn.addEventListener('click', async () => {
        const email = emailInput.value;
        if (!email) return alert("Please enter email");
        
        currentUser = await BackendService.login(email);
        userStatus.textContent = `Logged in as: ${currentUser.email}`;
        loginSection.classList.add('hidden');
        mainSection.classList.remove('hidden');
    });

    searchBtn.addEventListener('click', async () => {
        const query = document.getElementById('search-input').value;
        if (!query) return alert("Please enter search term");
        
        const results = await BackendService.searchBooks(query);
        displaySearchResults(results);
    });

    // Helper functions
    function displaySearchResults(books) {
        const resultsDiv = document.getElementById('search-results');
        resultsDiv.innerHTML = '';
        
        if (books.length === 0) {
            resultsDiv.innerHTML = '<p>No books found</p>';
            return;
        }

        books.forEach(book => {
            const bookDiv = document.createElement('div');
            bookDiv.className = 'book-item';
            bookDiv.innerHTML = `
                <div class="book-info">
                    <h3>${book.title}</h3>
                    <p>Author: ${book.author}</p>
                    <p>Genre: ${book.genre} | Year: ${book.year}</p>
                </div>
                <div class="book-actions">
                    <button class="borrow-btn" data-isbn="${book.isbn}">
                        ${book.available ? 'Borrow' : 'Unavailable'}
                    </button>
                </div>
            `;
            resultsDiv.appendChild(bookDiv);
        });
    }
});