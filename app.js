function showToast(severity,title,message,duration){
  const container=document.getElementById('toast-container'); if(!container)return;
  const toast=document.createElement('div');
  toast.className=`toast toast--${severity==='critical'?'critical':severity==='warning'?'warning':severity==='success'?'success':'info'}`;
  const icons={critical:'\ud83d\udea8',warning:'\u26a0\ufe0f',info:'\u2139\ufe0f',success:'\u2705'};
  const dur=duration||(severity==='critical'?0:5000);
  toast.innerHTML=`<span class="toast-icon">${icons[severity]||'\u2139\ufe0f'}</span><div class="toast-body"><div class="toast-title">${title}</div><div class="toast-msg">${message}</div></div><button class="toast-close" onclick="this.closest('.toast').remove()">\u2715</button>${dur>0?`<div class="toast-progress" style="animation-duration:${dur}ms;color:${severity==='critical'?'var(--red)':severity==='warning'?'var(--orange)':'var(--blue)'}"></div>`:''}`;
  container.appendChild(toast);
  if(dur>0)setTimeout(()=>{toast.classList.add('toast-exit');setTimeout(()=>toast.remove(),300);},dur);
}

// FIX: Update all showToast calls to use proper parameter order (severity, title, message, duration)
// Replace old calls like: showToast('message', 'success')
// With new calls like: showToast('success', 'Title', 'message', duration)