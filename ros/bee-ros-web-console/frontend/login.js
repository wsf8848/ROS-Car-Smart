(() => {
  const form = document.getElementById('login-form');
  const username = document.getElementById('username');
  const password = document.getElementById('password');
  const remember = document.getElementById('remember');
  const submit = document.getElementById('login-submit');
  const message = document.getElementById('login-message');
  const toggle = document.getElementById('toggle-password');
  const connectionDot = document.getElementById('connection-dot');
  const connectionText = document.getElementById('connection-text');

  const setMessage = (text, kind = 'error') => {
    message.textContent = text;
    message.className = text ? `message show${kind === 'info' ? ' info' : ''}` : 'message';
  };

  const setOnline = (online) => {
    connectionDot.classList.toggle('offline', !online);
    connectionText.textContent = online ? '网关在线 · 等待身份验证' : '无法连接车载网关';
  };

  toggle.addEventListener('click', () => {
    const show = password.type === 'password';
    password.type = show ? 'text' : 'password';
    toggle.textContent = show ? '隐藏' : '显示';
    toggle.setAttribute('aria-label', show ? '隐藏密码' : '显示密码');
    toggle.setAttribute('aria-pressed', String(show));
    password.focus();
  });

  form.addEventListener('submit', async (event) => {
    event.preventDefault();
    setMessage('');
    if (!username.value.trim() || !password.value) {
      setMessage('请输入管理员账号和密码。');
      (!username.value.trim() ? username : password).focus();
      return;
    }
    submit.disabled = true;
    submit.querySelector('span').textContent = '正在验证...';
    try {
      const response = await fetch('/api/auth/login', {
        method: 'POST',
        credentials: 'same-origin',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({username: username.value.trim(), password: password.value, remember: remember.checked}),
      });
      const result = await response.json().catch(() => ({}));
      if (!response.ok) {
        if (response.status === 429) throw new Error('尝试次数过多，请稍后再试。');
        if (response.status === 503) throw new Error('管理员凭据尚未配置，请联系设备维护人员。');
        throw new Error('账号或密码不正确。');
      }
      setMessage('验证通过，正在进入控制台...', 'info');
      window.location.replace('/');
    } catch (error) {
      setOnline(navigator.onLine);
      setMessage(error instanceof TypeError ? '无法连接车载网关，请检查网络。' : error.message);
      password.select();
    } finally {
      submit.disabled = false;
      submit.querySelector('span').textContent = '进入控制台';
    }
  });

  fetch('/api/auth/status', {credentials: 'same-origin', cache: 'no-store'})
    .then((response) => response.json())
    .then((status) => {
      setOnline(true);
      if (status.authenticated) window.location.replace('/');
      else username.focus();
    })
    .catch(() => { setOnline(false); username.focus(); });
})();
