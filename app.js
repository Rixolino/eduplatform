// Applicazione principale - Gestione pagine e logica
class CourseApp {
    constructor() {
        this.currentUser = null;
        this.currentPage = 'landing';
        this.init();
    }

    init() {
        this.setupEventListeners();
        this.checkAuthStatus();
    }

    setupEventListeners() {
        // Landing page buttons
        document.getElementById('btnHeroLogin')?.addEventListener('click', () => this.showPage('login'));
        document.getElementById('btnHeroRegister')?.addEventListener('click', () => this.showPage('registration'));
        document.getElementById('navLogin')?.addEventListener('click', () => this.showPage('login'));
        document.getElementById('navRegister')?.addEventListener('click', () => this.showPage('registration'));

        // Auth forms
        document.getElementById('loginForm')?.addEventListener('submit', (e) => this.handleLogin(e));
        document.getElementById('registrationForm')?.addEventListener('submit', (e) => this.handleRegistration(e));

        // Logout buttons
        document.querySelectorAll('#btnLogout').forEach(btn => {
            btn.addEventListener('click', () => this.handleLogout());
        });

        // Dashboard navigation
        document.querySelectorAll('[data-page]').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const page = link.dataset.page;
                this.showDashboardPage(page);
            });
        });

        // Course search and filters
        document.getElementById('searchCourses')?.addEventListener('input', () => this.searchCourses());
        document.getElementById('filterCategory')?.addEventListener('change', () => this.searchCourses());
        document.getElementById('filterDifficulty')?.addEventListener('change', () => this.searchCourses());

        // Course form
        document.getElementById('courseForm')?.addEventListener('submit', (e) => this.handleCreateCourse(e));
    }

    async checkAuthStatus() {
        const token = api.getToken();
        if (token) {
            try {
                const user = await api.getUserProfile();
                this.currentUser = user;
                this.showAuthenticatedUI();
            } catch (error) {
                console.error('Auth check failed:', error);
                api.clearToken();
                this.showPage('landing');
            }
        } else {
            this.showPage('landing');
        }
    }

    async handleLogin(e) {
        e.preventDefault();
        const username = document.getElementById('loginUsername').value;
        const password = document.getElementById('loginPassword').value;

        try {
            await api.login({ username, password });
            const user = await api.getUserProfile();
            this.currentUser = user;
            this.showAuthenticatedUI();
        } catch (error) {
            this.showAlert('Errore di login: ' + error.message, 'error');
        }
    }

    async handleRegistration(e) {
        e.preventDefault();
        const userData = {
            full_name: document.getElementById('regFullName').value,
            username: document.getElementById('regUsername').value,
            email: document.getElementById('regEmail').value,
            password: document.getElementById('regPassword').value,
            role: document.getElementById('regRole').value
        };

        try {
            await api.register(userData);
            this.showAlert('Registrazione completata! Accedi con le tue credenziali.', 'success');
            this.showPage('login');
        } catch (error) {
            this.showAlert('Errore di registrazione: ' + error.message, 'error');
        }
    }

    async handleLogout() {
        await api.logout();
        this.currentUser = null;
        this.showPage('landing');
    }

    showAuthenticatedUI() {
        const navbar = document.getElementById('navbarMenu');
        if (navbar) {
            navbar.innerHTML = `
                <li><span>Benvenuto, ${this.currentUser.full_name}</span></li>
                <li><a href="#" id="btnLogout">Logout</a></li>
            `;
            document.getElementById('btnLogout').addEventListener('click', () => this.handleLogout());
        }

        if (this.currentUser.role === 'student') {
            this.showPage('student-dashboard');
            this.loadStudentDashboard();
        } else if (this.currentUser.role === 'teacher') {
            this.showPage('teacher-dashboard');
            this.loadTeacherDashboard();
        } else if (this.currentUser.role === 'admin') {
            this.showPage('student-dashboard');
        }
    }

    showPage(pageName) {
        // Nascondere tutte le pagine
        document.querySelectorAll('.page').forEach(page => {
            page.classList.add('hidden');
        });

        // Mostrare la pagina richiesta
        const pageMap = {
            'landing': 'landingPage',
            'login': 'loginPage',
            'registration': 'registrationPage',
            'student-dashboard': 'studentDashboard',
            'teacher-dashboard': 'teacherDashboard'
        };

        const pageId = pageMap[pageName];
        if (pageId) {
            document.getElementById(pageId)?.classList.remove('hidden');
            this.currentPage = pageName;
        }
    }

    showDashboardPage(section) {
        const sections = {
            'dashboard': ['dashboardSummary', this.loadStudentDashboard.bind(this)],
            'courses': ['coursesCatalog', this.loadCoursesCatalog.bind(this)],
            'my-courses': ['myCourses', this.loadMyCourses.bind(this)],
            'progress': ['progressTracker', this.loadProgressTracker.bind(this)],
            'teacher-dashboard': ['teacherDashboardSummary', this.loadTeacherDashboard.bind(this)],
            'create-course': ['createCourseForm', () => {}],
            'students': ['studentList', this.loadStudentsList.bind(this)]
        };

        if (sections[section]) {
            document.querySelectorAll('.dashboard-section').forEach(s => {
                s.classList.add('hidden');
            });
            document.getElementById(sections[section][0])?.classList.remove('hidden');
            sections[section][1]();
        }
    }

    async loadStudentDashboard() {
        try {
            const enrollments = await api.getEnrollments();
            document.getElementById('userGreeting').textContent = this.currentUser.full_name;
            document.getElementById('statEnrolled').textContent = enrollments.length;
            
            const completed = enrollments.filter(e => e.status === 'completed').length;
            document.getElementById('statCompleted').textContent = completed;
        } catch (error) {
            console.error('Error loading dashboard:', error);
        }
    }

    async loadCoursesCatalog() {
        try {
            const courses = await api.getCourses();
            this.renderCoursesList(courses, 'coursesList', true);
        } catch (error) {
            console.error('Error loading courses:', error);
        }
    }

    async loadMyCourses() {
        try {
            const enrollments = await api.getEnrollments();
            const coursesList = document.getElementById('enrolledCoursesList');
            coursesList.innerHTML = '';

            for (const enrollment of enrollments) {
                const course = await api.getCourse(enrollment.course_id);
                const card = this.createCourseCard(course, false, enrollment);
                coursesList.appendChild(card);
            }
        } catch (error) {
            console.error('Error loading my courses:', error);
        }
    }

    async loadProgressTracker() {
        try {
            const enrollments = await api.getEnrollments();
            const container = document.getElementById('progressStats');
            container.innerHTML = '';

            enrollments.forEach(enrollment => {
                const progressDiv = document.createElement('div');
                progressDiv.className = 'progress-item';
                progressDiv.innerHTML = `
                    <h4>${enrollment.course_title || 'Corso'}</h4>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: ${enrollment.progress_percentage}%"></div>
                    </div>
                    <p>${enrollment.progress_percentage}% completato</p>
                `;
                container.appendChild(progressDiv);
            });
        } catch (error) {
            console.error('Error loading progress:', error);
        }
    }

    async loadTeacherDashboard() {
        try {
            const courses = await api.getCourses();
            const teacherCourses = courses.filter(c => c.teacher_id === this.currentUser.id);
            
            document.getElementById('teacherGreeting').textContent = this.currentUser.full_name;
            document.getElementById('teacherStatCourses').textContent = teacherCourses.length;
        } catch (error) {
            console.error('Error loading teacher dashboard:', error);
        }
    }

    async handleCreateCourse(e) {
        e.preventDefault();
        const formData = new FormData(document.getElementById('courseForm'));
        const courseData = Object.fromEntries(formData);

        try {
            await api.createCourse(courseData);
            this.showAlert('Corso creato con successo!', 'success');
            document.getElementById('courseForm').reset();
        } catch (error) {
            this.showAlert('Errore nella creazione del corso: ' + error.message, 'error');
        }
    }

    async searchCourses() {
        const search = document.getElementById('searchCourses')?.value || '';
        const category = document.getElementById('filterCategory')?.value || '';
        const difficulty = document.getElementById('filterDifficulty')?.value || '';

        try {
            const courses = await api.getCourses({ search, category, difficulty });
            this.renderCoursesList(courses, 'coursesList', true);
        } catch (error) {
            console.error('Error searching courses:', error);
        }
    }

    renderCoursesList(courses, containerId, showEnroll = false) {
        const container = document.getElementById(containerId);
        if (!container) return;

        container.innerHTML = '';

        if (courses.length === 0) {
            container.innerHTML = '<p>Nessun corso trovato</p>';
            return;
        }

        courses.forEach(course => {
            const card = this.createCourseCard(course, showEnroll);
            container.appendChild(card);
        });
    }

    createCourseCard(course, showEnroll = false, enrollment = null) {
        const card = document.createElement('div');
        card.className = 'course-card';

        const progressHtml = enrollment ? `
            <div class="course-progress">
                <div class="progress-bar">
                    <div class="progress-fill" style="width: ${enrollment.progress_percentage}%"></div>
                </div>
                <small>${enrollment.progress_percentage}% completato</small>
            </div>
        ` : '';

        const buttonHtml = showEnroll ? `
            <button class="btn btn-primary" onclick="app.enrollCourse(${course.id})">
                Iscriviti
            </button>
        ` : '';

        card.innerHTML = `
            <div class="course-card-header">
                <h3>${course.title}</h3>
                <p>${course.teacher_name || 'Docente'}</p>
            </div>
            <div class="course-card-body">
                <div class="course-meta">
                    <span>${course.num_lessons || 0} lezioni</span>
                    <span class="course-difficulty">${course.difficulty_level || 'N/A'}</span>
                </div>
                <p class="course-description">${course.description || 'Nessuna descrizione'}</p>
                ${progressHtml}
                <div class="course-card-footer">
                    ${buttonHtml}
                    <button class="btn btn-secondary" onclick="app.viewCourse(${course.id})">
                        Dettagli
                    </button>
                </div>
            </div>
        `;

        return card;
    }

    async enrollCourse(courseId) {
        try {
            await api.enrollCourse(courseId);
            this.showAlert('Iscritto al corso con successo!', 'success');
            this.loadMyCourses();
        } catch (error) {
            this.showAlert('Errore nell\'iscrizione: ' + error.message, 'error');
        }
    }

    viewCourse(courseId) {
        // Implementare visualizzazione dettagli corso
        console.log('View course:', courseId);
    }

    async loadStudentsList() {
        // Implementare lista studenti per docenti
    }

    showAlert(message, type = 'info') {
        const alertDiv = document.createElement('div');
        alertDiv.className = `alert alert-${type}`;
        alertDiv.textContent = message;
        
        const container = document.querySelector('.dashboard-content') || document.body;
        container.insertBefore(alertDiv, container.firstChild);

        setTimeout(() => alertDiv.remove(), 5000);
    }
}

// Inizializzare l'app quando il DOM è pronto
document.addEventListener('DOMContentLoaded', () => {
    window.app = new CourseApp();
});
