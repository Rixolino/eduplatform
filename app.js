// Applicazione principale - Gestione pagine e logica
class CourseApp {
  constructor() {
    this.currentUser = null;
    this.currentPage = "landing";
    this.init();
  }

  async init() {
    await this.loadPartials();
    this.setupEventListeners();
    this.checkAuthStatus();
  }

  async loadPartials() {
    try {
      const headerRes = await fetch("partials/header.html");
      if (headerRes.ok)
        document.getElementById("header-placeholder").innerHTML =
          await headerRes.text();

      const footerRes = await fetch("partials/footer.html");
      if (footerRes.ok)
        document.getElementById("footer-placeholder").innerHTML =
          await footerRes.text();
    } catch (e) {
      console.error("Error loading partials:", e);
    }
  }

  setupEventListeners() {
    // Escolta cambiamenti di hash per il routing
    window.addEventListener("hashchange", () => this.handleRoute());

    // Landing page buttons
    document
      .getElementById("btnHeroLogin")
      ?.addEventListener("click", () => (window.location.hash = "login"));
    document
      .getElementById("btnHeroRegister")
      ?.addEventListener("click", () => (window.location.hash = "register"));

    // Auth forms
    document
      .getElementById("loginForm")
      ?.addEventListener("submit", (e) => this.handleLogin(e));
    document
      .getElementById("registrationForm")
      ?.addEventListener("submit", (e) => this.handleRegistration(e));

    // Logout buttons
    document.querySelectorAll("#btnLogout").forEach((btn) => {
      btn.addEventListener("click", () => this.handleLogout());
    });

    // Dashboard navigation
    document.querySelectorAll("[data-page]").forEach((link) => {
      link.addEventListener("click", (e) => {
        e.preventDefault();
        const page = link.dataset.page;
        window.location.hash = page;
      });
    });

    // Course search and filters
    document
      .getElementById("searchCourses")
      ?.addEventListener("input", () => this.searchCourses());
    document
      .getElementById("filterCategory")
      ?.addEventListener("change", () => this.searchCourses());
    document
      .getElementById("filterDifficulty")
      ?.addEventListener("change", () => this.searchCourses());

    // Course form
    document
      .getElementById("courseForm")
      ?.addEventListener("submit", (e) => this.handleCreateCourse(e));

    // Task form
    document
      .getElementById("taskForm")
      ?.addEventListener("submit", (e) => this.handleCreateTask(e));

    // Focus Mode / Pomodoro
    document.getElementById("btnExitFocus")?.addEventListener("click", () => {
      document.getElementById("focusModeOverlay").classList.add("hidden");
      this.pausePomodoro();
    });
    document
      .getElementById("btnTimerStart")
      ?.addEventListener("click", () => this.startPomodoro());
    document
      .getElementById("btnTimerPause")
      ?.addEventListener("click", () => this.pausePomodoro());
    document
      .getElementById("btnTimerReset")
      ?.addEventListener("click", () => this.resetPomodoro());

    // Study Buddy
    document
      .getElementById("closeBuddyModal")
      ?.addEventListener("click", () => {
        document.getElementById("studyBuddyModal").classList.add("hidden");
      });
  }

  async checkAuthStatus() {
    const token = api.getToken();
    if (token) {
      try {
        const user = await api.getUserProfile();
        this.currentUser = user;
        this.showAuthenticatedUI();
      } catch (error) {
        console.error("Auth check failed:", error);
        api.clearToken();
        this.currentUser = null;
        this.handleRoute();
      }
    } else {
      this.currentUser = null;
      this.handleRoute();
    }
  }

  async handleLogin(e) {
    e.preventDefault();
    const username = document.getElementById("loginUsername").value;
    const password = document.getElementById("loginPassword").value;

    try {
      await api.login({ username, password });
      const user = await api.getUserProfile();
      this.currentUser = user;
      this.showAuthenticatedUI();
    } catch (error) {
      this.showAlert("Errore di login: " + error.message, "error");
    }
  }

  async handleRegistration(e) {
    e.preventDefault();

    const password = document.getElementById("regPassword").value;
    if (password.length < 6) {
      this.showAlert(
        "Attenzione: La password deve avere almeno 6 caratteri.",
        "error",
      );
      return;
    }

    const userData = {
      full_name: document.getElementById("regFullName").value,
      username: document.getElementById("regUsername").value,
      email: document.getElementById("regEmail").value,
      password: password,
      role: document.getElementById("regRole").value,
    };

    try {
      await api.register(userData);
      this.showAlert(
        "Registrazione completata! Accedi con le tue credenziali.",
        "success",
      );
      window.location.hash = "login";
    } catch (error) {
      this.showAlert("Errore di registrazione: " + error.message, "error");
    }
  }

  async handleLogout() {
    await api.logout();
    this.currentUser = null;
    window.location.reload();
  }

  showAuthenticatedUI() {
    const navbar = document.getElementById("navbarMenu");
    if (navbar) {
      navbar.innerHTML = `
                <li><span>Benvenuto, ${this.currentUser.username}</span></li>
                <li><a href="#" id="btnLogout" class="logout-btn"><i class="fas fa-sign-out-alt"></i> Logout</a></li>
            `;
      document
        .getElementById("btnLogout")
        .addEventListener("click", () => this.handleLogout());
    }

    this.handleRoute();
  }

  handleRoute() {
    const hash = window.location.hash.substring(1) || "";

    if (!this.currentUser) {
      // Unauthenticated routes
      if (hash === "login") {
        this.showPage("login");
      } else if (hash === "register" || hash === "registration") {
        this.showPage("registration");
      } else {
        this.showPage("landing");
        if (hash !== "" && hash !== "home") {
          window.location.hash = "home";
        }
      }
    } else {
      // Authenticated routes
      if (this.currentUser.role === "teacher") {
        this.showPage("teacher-dashboard");
      } else {
        this.showPage("student-dashboard");
      }

      const validSections = [
        "dashboard",
        "courses",
        "paths",
        "my-courses",
        "progress",
        "teacher-dashboard",
        "teacher-courses",
        "create-course",
        "students",
      ];

      if (validSections.includes(hash)) {
        this.showDashboardPage(hash);
      } else {
        const defaultSection =
          this.currentUser.role === "teacher"
            ? "teacher-dashboard"
            : "dashboard";

        // Only set hash if we were trying to access a public route or empty hash
        if (
          hash === "" ||
          hash === "login" ||
          hash === "register" ||
          hash === "landing"
        ) {
          window.location.hash = defaultSection;
        } else {
          this.showDashboardPage(defaultSection);
        }
      }
    }
  }

  showPage(pageName) {
    // Nascondere tutte le pagine
    document.querySelectorAll(".page").forEach((page) => {
      page.classList.add("hidden");
    });

    // Mostrare la pagina richiesta
    const pageMap = {
      landing: "landingPage",
      login: "loginPage",
      registration: "registrationPage",
      "student-dashboard": "studentDashboard",
      "teacher-dashboard": "teacherDashboard",
    };

    const pageId = pageMap[pageName];
    if (pageId) {
      document.getElementById(pageId)?.classList.remove("hidden");
      this.currentPage = pageName;
    }
  }

  showDashboardPage(section) {
    const sections = {
      dashboard: ["dashboardSummary", this.loadStudentDashboard.bind(this)],
      courses: ["coursesCatalog", this.loadCoursesCatalog.bind(this)],
      paths: ["pathsCatalog", this.loadPathsCatalog.bind(this)],
      "my-courses": ["myCourses", this.loadMyCourses.bind(this)],
      progress: ["progressTracker", this.loadProgressTracker.bind(this)],
      "teacher-dashboard": [
        "teacherDashboardSummary",
        this.loadTeacherDashboard.bind(this),
      ],
      "teacher-courses": [
        "teacherManageCourses",
        this.loadTeacherCourses.bind(this),
      ],
      "create-course": ["createCourseFormContainer", () => {}],
      students: ["teacherStudentList", this.loadStudentsList.bind(this)],
    };

    if (sections[section]) {
      document.querySelectorAll(".dashboard-section").forEach((s) => {
        s.classList.add("hidden");
      });
      const targetId = sections[section][0];
      const targetEl = document.getElementById(targetId);
      if (targetEl) {
        targetEl.classList.remove("hidden");
        sections[section][1]();
      } else {
        console.error(`Dashboard section element not found: ${targetId}`);
      }
    }
  }

  async loadStudentDashboard() {
    try {
      const response = await api.getEnrollments();
      const enrollments = Array.isArray(response)
        ? response
        : response.enrollments || [];

      document.getElementById("userGreeting").textContent =
        this.currentUser.full_name;
      document.getElementById("statEnrolled").textContent = enrollments.length;

      const completed = enrollments.filter(
        (e) => e.status === "completed",
      ).length;
      document.getElementById("statCompleted").textContent = completed;

      // Generate Skill Graph
      this.renderSkillGraph(enrollments);
    } catch (error) {
      console.error("Error loading dashboard:", error);
    }
  }

  async renderSkillGraph(enrollments) {
    try {
      const res = await api.getUserSkills();
      const skills = res.skills || [];

      const container = document.getElementById("skillGraphCanvas");
      if (!container) return;

      if (skills.length === 0) {
        container.innerHTML = "<p>Nessuna competenza acquisita ancora.</p>";
        return;
      }

      let mermaidCode = "graph BT\n";
      mermaidCode += '  root(("Selettore Competenze"))\n';

      skills.forEach((skill, idx) => {
        const nodeId = `skill_${idx}`;
        const score = parseFloat(skill.score).toFixed(1);
        mermaidCode += `  ${nodeId}["${skill.category} (Lv. ${score})"]\n`;
        mermaidCode += `  ${nodeId} --> root\n`;
      });

      container.innerHTML = `<div class="mermaid">${mermaidCode}</div>`;
      if (window.mermaid) {
        mermaid.init(undefined, container.querySelectorAll(".mermaid"));
      }
    } catch (e) {
      console.error("Error rendering skill graph", e);
    }
  }

  async loadPathsCatalog() {
    try {
      const res = await api.getPaths();
      const paths = res.paths || [];
      const container = document.getElementById("pathsList");
      if (!container) return;

      container.innerHTML = "";
      if (paths.length === 0) {
        container.innerHTML = "<p>Nessun percorso trovato.</p>";
        return;
      }

      paths.forEach((p) => {
        const card = document.createElement("div");
        card.className = "course-card";
        card.innerHTML = `
                    <div class="course-card-header">
                        <h3>${p.title}</h3>
                    </div>
                    <div class="course-card-body">
                        <p class="course-description">${p.description || ""}</p>
                        <div class="course-card-footer" style="margin-top: 1rem;">
                            <button class="btn btn-primary" onclick="app.viewPath(${p.id})">Vedi Percorso</button>
                        </div>
                    </div>
                `;
        container.appendChild(card);
      });
    } catch (e) {
      console.error("Error loading paths", e);
    }
  }

  async viewPath(pathId) {
    try {
      const res = await api.getPathDetails(pathId);
      this.showAlert(
        "Percorso caricato: " + res.courses.map((c) => c.title).join(" -> "),
        "success",
      );
    } catch (e) {
      this.showAlert("Errore nel caricare il percorso", "error");
    }
  }

  async loadCoursesCatalog() {
    try {
      const response = await api.getCourses();
      const coursesArray = response.courses || []; // Estrai l'array
      this.renderCoursesList(coursesArray, "coursesList", true);
    } catch (error) {
      console.error("Error loading courses:", error);
    }
  }

  async loadMyCourses() {
    try {
      const response = await api.getEnrollments();
      const enrollments = Array.isArray(response)
        ? response
        : response.enrollments || [];
      const coursesList = document.getElementById("enrolledCoursesList");
      coursesList.innerHTML = "";

      for (const enrollment of enrollments) {
        const course = await api.getCourse(enrollment.course_id);
        const card = this.createCourseCard(course, false, enrollment);
        coursesList.appendChild(card);
      }
    } catch (error) {
      console.error("Error loading my courses:", error);
    }
  }

  async loadProgressTracker() {
    try {
      const response = await api.getEnrollments();
      const enrollments = Array.isArray(response)
        ? response
        : response.enrollments || [];
      const container = document.getElementById("progressStats");
      container.innerHTML = "";

      enrollments.forEach((enrollment) => {
        const progressDiv = document.createElement("div");
        progressDiv.className = "progress-item";
        progressDiv.innerHTML = `
                    <h4>${enrollment.course_title || "Corso"}</h4>
                    <div class="progress-bar">
                        <div class="progress-fill" style="width: ${enrollment.progress_percentage}%"></div>
                    </div>
                    <p>${enrollment.progress_percentage}% completato</p>
                `;
        container.appendChild(progressDiv);
      });
    } catch (error) {
      console.error("Error loading progress:", error);
    }
  }

  async loadTeacherDashboard() {
    try {
      const response = await api.getCourses();
      const coursesArray = response.courses || []; // Estrai l'array
      const currentTeacherId =
        this.currentUser.user_id || this.currentUser.id || 0;
      const teacherCourses = coursesArray.filter(
        (c) => c.teacher_id == currentTeacherId,
      );

      document.getElementById("teacherGreeting").textContent =
        this.currentUser.full_name;
      document.getElementById("teacherStatCourses").textContent =
        teacherCourses.length;

      try {
        const studentsRes = await api.getTeacherStudents();
        const uniqueStudents = new Set(
          (studentsRes.students || []).map((s) => s.id),
        );
        document.getElementById("teacherStatStudents").textContent =
          uniqueStudents.size;
      } catch (e) {}

      const coursesList = document.getElementById("teacherCoursesList");
      if (coursesList) {
        this._renderTeacherCourseCards(coursesList, teacherCourses);
      }
    } catch (error) {
      console.error("Error loading teacher dashboard:", error);
    }
  }

  async loadTeacherCourses() {
    try {
      const response = await api.getCourses();
      const coursesArray = response.courses || []; // Estrai l'array
      const currentTeacherId =
        this.currentUser.user_id || this.currentUser.id || 0;
      const teacherCourses = coursesArray.filter(
        (c) => c.teacher_id == currentTeacherId,
      );
      const container = document.getElementById("managedCoursesList");
      if (container) {
        this._renderTeacherCourseCards(container, teacherCourses);
      }
    } catch (error) {
      console.error("Error loading teacher courses:", error);
    }
  }

  showTaskCreation(courseId) {
    // Simple UI switch to show task form
    document
      .getElementById("createCourseFormContainer")
      ?.classList.add("hidden");
    document.getElementById("taskFormContainer")?.classList.remove("hidden");
    document.getElementById("taskCourseId").value = courseId;
  }

  async handleCreateTask(e) {
    e.preventDefault();
    const formEl = document.getElementById("taskForm");
    const formData = new FormData(formEl);
    const taskData = Object.fromEntries(formData);

    // FIX: Rimuovi virgolette doppie e accapo per non rompere il JSON del backend C
    if (taskData.title)
      taskData.title = taskData.title.replace(/["\n\r]/g, " ");
    if (taskData.description)
      taskData.description = taskData.description.replace(/["\n\r]/g, " ");

    try {
      await api.createTask(taskData);
      this.showAlert("Task creato con successo!", "success");
      formEl.reset();
    } catch (error) {
      this.showAlert(
        "Errore nella creazione del task: " + error.message,
        "error",
      );
    }
  }

  async handleCreateCourse(e) {
    e.preventDefault();
    if (
      !this.currentUser ||
      !["teacher", "admin"].includes(this.currentUser.role)
    ) {
      this.showAlert(
        "Permesso negato: solo docenti o amministratori possono creare corsi",
        "error",
      );
      return;
    }

    const formEl = document.getElementById("courseForm");
    const formData = new FormData(formEl);
    const courseData = Object.fromEntries(formData);

    const teacherId = this.currentUser.user_id || this.currentUser.id;
    if (teacherId) courseData.teacher_id = String(teacherId);

    if (courseData.duration_hours)
      courseData.duration_hours = parseFloat(courseData.duration_hours);
    if (courseData.num_lessons)
      courseData.num_lessons = parseInt(courseData.num_lessons);

    // FIX: Rimuovi virgolette doppie e accapo per non rompere il JSON del backend C
    if (courseData.title)
      courseData.title = courseData.title.replace(/["\n\r]/g, " ");
    if (courseData.description)
      courseData.description = courseData.description.replace(/["\n\r]/g, " ");

    try {
      await api.createCourse(courseData);
      this.showAlert("Corso creato con successo!", "success");
      formEl.reset();
      this.loadTeacherDashboard();
    } catch (error) {
      this.showAlert(
        "Errore nella creazione del corso: " + error.message,
        "error",
      );
    }
  }

  async searchCourses() {
    const search = document.getElementById("searchCourses")?.value || "";
    const category = document.getElementById("filterCategory")?.value || "";
    const difficulty = document.getElementById("filterDifficulty")?.value || "";

    try {
      const response = await api.getCourses({ search, category, difficulty });
      const coursesArray = response.courses || []; // Estrai l'array
      this.renderCoursesList(coursesArray, "coursesList", true);
    } catch (error) {
      console.error("Error searching courses:", error);
    }
  }

  renderCoursesList(courses, containerId, showEnroll = false) {
    const container = document.getElementById(containerId);
    if (!container) return;

    container.innerHTML = "";

    if (courses.length === 0) {
      container.innerHTML = "<p>Nessun corso trovato</p>";
      return;
    }

    courses.forEach((course) => {
      const card = this.createCourseCard(course, showEnroll);
      container.appendChild(card);
    });
  }

  _renderTeacherCourseCards(container, courses) {
    if (!container || !courses) return;

    container.innerHTML = "";

    if (courses.length === 0) {
      container.innerHTML = "<p>Nessun corso gestito</p>";
      return;
    }

    const cards = courses.map((course) => this.createCourseCard(course, false));

    cards.forEach((card) => {
      container.appendChild(card);
    });
  }

  createCourseCard(course, showEnroll = false, enrollment = null) {
    const card = document.createElement("div");
    card.className = "course-card";

    const progressHtml = enrollment
      ? `
            <div class="course-progress">
                <div class="progress-bar">
                    <div class="progress-fill" style="width: ${enrollment.progress_percentage}%"></div>
                </div>
                <small>${enrollment.progress_percentage}% completato</small>
            </div>
        `
      : "";

    const actionHtml = showEnroll
      ? `
            <button class="btn btn-secondary" onclick="app.viewCourse(${course.id})">
                Dettagli
            </button>
            <button class="btn btn-primary" onclick="app.enrollCourse(${course.id})">
                Iscriviti
            </button>
        `
      : `
            <button class="btn btn-secondary" onclick="app.viewCourse(${course.id})">
                Apri Corso
            </button>
            <button class="btn btn-success" onclick="app.openBuddyModal(${course.id})">
                <i class="fas fa-user-friends"></i> Buddy
            </button>
        `;

    card.innerHTML = `
            <div class="course-card-header" style="cursor: pointer;" onclick="app.viewCourse(${course.id})">
                <h3 style="color: var(--primary-color);">${course.title}</h3>
                <p>${course.teacher_name || "Docente"}</p>
            </div>
            <div class="course-card-body">
                <div class="course-meta">
                    <span>${course.num_lessons || 0} lezioni</span>
                    <span class="course-difficulty">${course.difficulty_level || "N/A"}</span>
                </div>
                <p class="course-description">${course.description || "Nessuna descrizione"}</p>
                ${progressHtml}
                <div class="course-card-footer" style="display: flex; gap: 0.5rem; flex-wrap: wrap;">
                    ${actionHtml}
                </div>
            </div>
        `;

    return card;
  }

  async enrollCourse(courseId) {
    try {
      await api.enrollCourse(courseId);
      this.showAlert("Iscritto al corso con successo!", "success");
      this.loadMyCourses();
    } catch (error) {
      this.showAlert("Errore nell'iscrizione: " + error.message, "error");
    }
  }

  async viewCourse(courseId) {
    console.log("View course:", courseId);
    try {
      const course = await api.getCourse(courseId);
      const tasks = await api.getCourseTasks(courseId);

      if (course) {
        // Determine if user is teacher or student
        const isTeacher =
          this.currentUser && this.currentUser.role === "teacher";

        // Use appropriate IDs based on user role
        const titleId = isTeacher ? "cdTitleTeacher" : "cdTitle";
        const categoryId = isTeacher ? "cdCategoryTeacher" : "cdCategory";
        const difficultyId = isTeacher ? "cdDifficultyTeacher" : "cdDifficulty";
        const durationId = isTeacher ? "cdDurationTeacher" : "cdDuration";
        const lessonsId = isTeacher ? "cdLessonsTeacher" : "cdLessons";
        const descId = isTeacher ? "cdDescriptionTeacher" : "cdDescription";
        const taskListId = isTeacher ? "cdTaskListTeacher" : "cdTaskList";
        const detailsId = isTeacher ? "courseDetailsTeacher" : "courseDetails";

        // Populate course details
        document.getElementById(titleId).textContent = course.title;
        document.getElementById(categoryId).innerHTML =
          `<i class="fas fa-tag"></i> ${course.category || "N/D"}`;
        document.getElementById(difficultyId).innerHTML =
          `<i class="fas fa-signal"></i> ${course.difficulty_level || "N/D"}`;
        document.getElementById(durationId).innerHTML =
          `<i class="fas fa-clock"></i> ${course.duration_hours || 0} ore`;
        document.getElementById(lessonsId).innerHTML =
          `<i class="fas fa-book"></i> ${course.num_lessons || 0} lezioni`;
        document.getElementById(descId).textContent =
          course.description || "Nessuna descrizione disponibile.";

        // Tasks
        const taskList = document.getElementById(taskListId);
        taskList.innerHTML = "";
        if (tasks && tasks.tasks && tasks.tasks.length > 0) {
          tasks.tasks.forEach((task) => {
            const taskCard = document.createElement("div");
            taskCard.className = "course-card";
            taskCard.style.cursor = "pointer";
            taskCard.onclick = () => this.viewTask(task.id);
            taskCard.innerHTML = `
                            <div class="course-content">
                                <h3 class="course-title">${task.title} <span class="badge" style="font-size: 0.7em; background: var(--bg-color); color: var(--primary-color); float: right;">${task.task_type.toUpperCase()}</span></h3>
                                <p class="course-description">${task.description || "Nessuna descrizione"}</p>
                                <div class="course-meta">
                                    <span><i class="fas fa-calendar"></i> Scadenza: ${task.due_date ? new Date(task.due_date).toLocaleDateString() : "N/D"}</span>
                                    <span><i class="fas fa-star"></i> Punti: ${task.points || 0}</span>
                                </div>
                            </div>
                        `;
            taskList.appendChild(taskCard);
          });
        } else {
          taskList.innerHTML =
            '<p style="color: var(--text-light);">Nessun task assegnato in questo corso.</p>';
        }

        // Setup buttons based on role
        if (isTeacher) {
          const btnManageTasks = document.getElementById("cdManageTasksBtn");
          btnManageTasks.onclick = () => {
            this.showTaskCreation(courseId);
          };
        } else {
          // Setup Focus Mode button for students
          const btnFocus = document.getElementById("cdFocusBtn");
          if (btnFocus) {
            btnFocus.onclick = () => {
              const overlay = document.getElementById("focusModeOverlay");
              if (overlay) {
                document.getElementById("focusCourseTitle").textContent =
                  course.title;
                document.getElementById("focusLessonTitle").textContent =
                  "Sessione Formativa (Focus Mode)";
                overlay.classList.remove("hidden");

                // init pomodoro
                this.pomodoroTime = 25 * 60;
                this.updatePomodoroDisplay();
                document.getElementById("pomodoroState").textContent =
                  "Fase di Studio 🚀";
              }
            };
          }
        }

        // Remember current visible section to go back
        if (isTeacher) {
          this.previousDashboardSection = Array.from(
            document.querySelectorAll("#teacherDashboard .dashboard-section"),
          ).find((s) => !s.classList.contains("hidden"))?.id;
        } else {
          this.previousDashboardSection = Array.from(
            document.querySelectorAll("#studentDashboard .dashboard-section"),
          ).find((s) => !s.classList.contains("hidden"))?.id;
        }

        // Show the course details section
        const detailsElement = document.getElementById(detailsId);
        if (detailsElement) {
          if (isTeacher) {
            document
              .querySelectorAll("#teacherDashboard .dashboard-section")
              .forEach((s) => s.classList.add("hidden"));
          } else {
            document
              .querySelectorAll("#studentDashboard .dashboard-section")
              .forEach((s) => s.classList.add("hidden"));
          }
          detailsElement.classList.remove("hidden");
        }
      }
    } catch (e) {
      console.error(e);
      this.showAlert("Impossibile caricare i dettagli del corso", "error");
    }
  }

  closeCourseDetails() {
    // Hide both possible course details sections
    const courseDetails = document.getElementById("courseDetails");
    const courseDetailsTeacher = document.getElementById(
      "courseDetailsTeacher",
    );

    if (courseDetails) courseDetails.classList.add("hidden");
    if (courseDetailsTeacher) courseDetailsTeacher.classList.add("hidden");

    if (this.previousDashboardSection) {
      document
        .getElementById(this.previousDashboardSection)
        .classList.remove("hidden");
    } else {
      // fallback
      this.showDashboardPage(
        this.currentUser.role === "teacher" ? "teacher-courses" : "my-courses",
      );
    }
  }

  async openBuddyModal(courseId) {
    try {
      const res = await api.getCourseBuddies(courseId);
      const buddies = res.buddies || [];

      const modal = document.getElementById("studyBuddyModal");
      const list = document.getElementById("buddiesList");

      list.innerHTML = "";

      if (buddies.length === 0) {
        list.innerHTML =
          "<p>Nessun compagno trovato per questo corso al momento.</p>";
      } else {
        buddies.forEach((b) => {
          // avoid showing ourselves if possible (mock might not match)
          if (this.currentUser && b.id === this.currentUser.id) return;

          const card = document.createElement("div");
          card.className = "buddy-card";
          card.innerHTML = `
                        <div class="buddy-info">
                            <strong><i class="fas fa-user-circle"></i> ${b.full_name || b.username}</strong>
                            <span>Progresso: ${b.progress_percentage}%</span>
                        </div>
                        <div class="buddy-action">
                            <button class="btn btn-primary" onclick="app.showAlert('Richiesta inviata a ${b.username}', 'success')">Connetti</button>
                        </div>
                    `;
          list.appendChild(card);
        });
      }

      modal.classList.remove("hidden");
    } catch (e) {
      this.showAlert("Errore nel caricamento compagni", "error");
    }
  }

  startPomodoro() {
    if (this.pomodoroInterval) return;
    this.pomodoroInterval = setInterval(() => {
      if (this.pomodoroTime > 0) {
        this.pomodoroTime--;
        this.updatePomodoroDisplay();
      } else {
        this.pausePomodoro();
        document.getElementById("pomodoroState").textContent = "Pausa! ☕";
        this.pomodoroTime = 5 * 60; // 5 min break
        this.updatePomodoroDisplay();
      }
    }, 1000);
  }

  pausePomodoro() {
    clearInterval(this.pomodoroInterval);
    this.pomodoroInterval = null;
  }

  resetPomodoro() {
    this.pausePomodoro();
    this.pomodoroTime = 25 * 60;
    this.updatePomodoroDisplay();
    document.getElementById("pomodoroState").textContent = "Fase di Studio 🚀";
  }

  updatePomodoroDisplay() {
    const min = Math.floor(this.pomodoroTime / 60)
      .toString()
      .padStart(2, "0");
    const sec = (this.pomodoroTime % 60).toString().padStart(2, "0");
    const disp = document.getElementById("pomodoroDisplay");
    if (disp) disp.textContent = `${min}:${sec}`;
  }

  async loadStudentsList() {
    const container = document.getElementById("teacherStudentsContainer");
    if (!container) return;
    container.innerHTML = "<p>Caricamento studenti...</p>";
    try {
      const res = await api.getTeacherStudents();
      const students = res.students || [];

      if (students.length === 0) {
        container.innerHTML =
          '<p style="text-align:center; padding: 2rem; color: var(--text-secondary);">Nessuno studente iscritto ai tuoi corsi al momento.</p>';
        return;
      }

      container.innerHTML = "";
      students.forEach((s) => {
        const div = document.createElement("div");
        div.className = "course-card";
        div.innerHTML = `
                    <div class="course-card-header">
                        <h3><i class="fas fa-user-graduate"></i> ${s.full_name || s.username}</h3>
                    </div>
                    <div class="course-card-body">
                        <p><strong>Username:</strong> ${s.username}</p>
                        <p><strong>Corso:</strong> ${s.course_title}</p>
                        <p><strong>Progresso:</strong> ${s.progress_percentage}%</p>
                        <p><strong>Stato:</strong> ${s.status}</p>
                    </div>
                `;
        container.appendChild(div);
      });
    } catch (error) {
      container.innerHTML =
        '<p style="color:red;">Errore nel caricamento degli studenti.</p>';
    }
  }

  showAlert(message, type = "info") {
    const alertDiv = document.createElement("div");
    alertDiv.className = `alert alert-${type}`;
    alertDiv.textContent = message;

    const container =
      document.querySelector(".dashboard-content") || document.body;
    container.insertBefore(alertDiv, container.firstChild);

    setTimeout(() => alertDiv.remove(), 5000);
  }

  async viewTask(taskId) {
    try {
      const task = await api.getTaskDetails(taskId);
      this.renderTaskDetails(task);
      this.showPage("task-details");
    } catch (error) {
      this.showAlert(
        "Errore nel caricamento del task: " + error.message,
        "error",
      );
    }
  }

  renderTaskDetails(task) {
    const container = document.getElementById("task-detail-view");
    if (!container) return;

    container.innerHTML = `
        <div class="task-detail-container">
            <button onclick="app.showDashboardPage()" class="btn-back">← Torna</button>
            <h2>${task.title}</h2>
            <div class="task-meta">
                <span><strong>Scadenza:</strong> ${task.due_date || "Non specificata"}</span>
                <span><strong>Punti:</strong> ${task.points}</span>
            </div>
            <div class="task-description">${task.description}</div>

            <div class="submission-area">
                <h3>Consegna Task</h3>
                <form id="submission-form">
                    <textarea id="submission-text" placeholder="Descrivi la tua soluzione..."></textarea>
                    <div class="file-upload">
                        <input type="file" id="submission-file" multiple>
                        <div id="file-list"></div>
                    </div>
                    <button type="submit" class="btn btn-primary">Invia Consegna</button>
                </form>
            </div>
        </div>
    `;

    const form = document.getElementById("submission-form");
    if (form) {
      form.onsubmit = (e) => this.handleTaskSubmission(e, task.id);
    }
  }

  async handleTaskSubmission(e, taskId) {
    e.preventDefault();
    const text = document.getElementById("submission-text").value;
    const fileInput = document.getElementById("submission-file");
    const files = fileInput.files;
    const uploadedFiles = [];

    try {
      for (let file of files) {
        const uploadRes = await api.uploadFile(file);
        uploadedFiles.push(uploadRes.file_id);
      }

      await api.submitSubmission({
        task_id: taskId,
        content: text,
        attachments: uploadedFiles,
      });

      this.showAlert("Consegna inviata con successo!", "success");
      this.viewTask(taskId); // Refresh
    } catch (error) {
      this.showAlert("Errore durante l'invio: " + error.message, "error");
    }
  }

  openMediaViewer(url, type) {
    const viewer = document.getElementById("media-viewer");
    const content = document.getElementById("media-viewer-content");
    if (!viewer || !content) return;

    viewer.classList.remove("hidden");

    if (type.includes("image")) {
      content.innerHTML = `<img src="${url}" class="viewer-img">`;
    } else if (type.includes("video")) {
      content.innerHTML = `<video src="${url}" controls autoplay class="viewer-video"></video>`;
    } else if (type.includes("audio")) {
      content.innerHTML = `<audio src="${url}" controls autoplay class="viewer-audio"></audio>`;
    } else {
      content.innerHTML = `<pre class="viewer-text">${this.fetchFileText(url)}</pre>`;
    }
  }

  async fetchFileText(url) {
    try {
      const res = await fetch(url);
      return await res.text();
    } catch (e) {
      return "Errore nel caricamento del file di testo.";
    }
  }

  closeMediaViewer() {
    document.getElementById("media-viewer")?.classList.add("hidden");
  }
}

// Inizializzare l'app quando il DOM è pronto
document.addEventListener("DOMContentLoaded", () => {
  window.app = new CourseApp();
});
