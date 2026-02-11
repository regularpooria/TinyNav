class DatasetEditor {
    constructor() {
        this.frames = [];
        this.currentFrame = 0;
        this.isPlaying = false;
        this.fps = 15;
        this.metadata = { width: 25, height: 25, binningFactor: 4 };
        this.selection = { start: null, end: null };
        this.history = [];
        this.timelineZoom = 1;
        this.timelineScroll = 0;
        
        this.setupUI();
        this.setupTimeline();
    }

    setupUI() {
        this.canvas = document.getElementById('depthCanvas');
        this.ctx = this.canvas.getContext('2d');
        
        document.getElementById('fileInput').addEventListener('change', (e) => this.loadFile(e));
        document.getElementById('playPauseBtn').addEventListener('click', () => this.togglePlayPause());
        document.getElementById('prevFrameBtn').addEventListener('click', () => this.prevFrame());
        document.getElementById('nextFrameBtn').addEventListener('click', () => this.nextFrame());
        document.getElementById('speedSlider').addEventListener('input', (e) => this.updateSpeed(e));
        document.getElementById('zoomSlider').addEventListener('input', (e) => this.updateZoom(e));
        document.getElementById('cutBtn').addEventListener('click', () => this.cutSelection());
        document.getElementById('deleteBtn').addEventListener('click', () => this.deleteSelection());
        document.getElementById('clearSelectionBtn').addEventListener('click', () => this.clearSelection());
        document.getElementById('undoBtn').addEventListener('click', () => this.undo());
        document.getElementById('exportBtn').addEventListener('click', () => this.exportData());
    }

    setupTimeline() {
        this.timelineCanvas = document.getElementById('timelineCanvas');
        this.timelineCtx = this.timelineCanvas.getContext('2d');
        
        const rect = this.timelineCanvas.getBoundingClientRect();
        this.timelineCanvas.width = rect.width;
        this.timelineCanvas.height = 120;
        
        this.isDragging = false;
        this.dragStart = null;
        
        this.timelineCanvas.addEventListener('mousedown', (e) => this.onTimelineMouseDown(e));
        this.timelineCanvas.addEventListener('mousemove', (e) => this.onTimelineMouseMove(e));
        this.timelineCanvas.addEventListener('mouseup', (e) => this.onTimelineMouseUp(e));
        this.timelineCanvas.addEventListener('click', (e) => this.onTimelineClick(e));
        this.timelineCanvas.addEventListener('wheel', (e) => this.onTimelineWheel(e));
        
        // Scrollbar
        this.timelineScrollbar = document.getElementById('timelineScrollbar');
        this.timelineScrollbar.addEventListener('input', (e) => {
            this.timelineScroll = parseFloat(e.target.value);
            this.drawTimeline();
        });
    }

    async loadFile(event) {
        const file = event.target.files[0];
        if (!file) return;

        const text = await file.text();
        const lines = text.split('\n').filter(line => line.trim() && !line.startsWith('#'));
        
        this.frames = [];
        
        for (const line of lines) {
            const values = line.split(',').map(v => parseFloat(v.trim()));
            if (values.length < 6) continue;
            
            const width = values[3];
            const height = values[4];
            
            if (!width || !height || width <= 0 || height <= 0 || !Number.isFinite(width) || !Number.isFinite(height)) {
                console.warn(`Skipping frame with invalid dimensions: ${width}x${height}`);
                continue;
            }
            
            const frame = {
                frameNum: values[0],
                steering: values[1],
                throttle: values[2],
                width: width,
                height: height,
                data: values.slice(5),
                active: true
            };
            
            this.frames.push(frame);
            this.metadata.width = frame.width;
            this.metadata.height = frame.height;
        }
        
        if (this.frames.length > 0) {
            this.currentFrame = 0;
            this.renderFrame();
            this.drawTimeline();
            document.getElementById('exportBtn').disabled = false;
        }
    }

    renderFrame() {
        if (this.frames.length === 0) return;
        
        const frame = this.frames[this.currentFrame];
        const w = frame.width;
        const h = frame.height;
        const data = frame.data;
        
        if (!w || !h || w <= 0 || h <= 0) {
            console.error(`Invalid frame dimensions: ${w}x${h}`);
            return;
        }
        
        // Rotate 90 degrees clockwise (k=-1): swap dimensions
        const imageData = this.ctx.createImageData(h, w);
        
        for (let row = 0; row < h; row++) {
            for (let col = 0; col < w; col++) {
                const srcIdx = row * w + col;
                const depth = data[srcIdx];
                const normalized = Math.max(0, Math.min(1, (depth - 100) / (2000 - 100)));
                const color = this.depthToColor(normalized);
                
                // Rotate 90 degrees clockwise: (row, col) -> (col, h-1-row)
                const newRow = col;
                const newCol = h - 1 - row;
                const dstIdx = (newRow * h + newCol) * 4;
                
                imageData.data[dstIdx] = color.r;
                imageData.data[dstIdx + 1] = color.g;
                imageData.data[dstIdx + 2] = color.b;
                imageData.data[dstIdx + 3] = 255;
            }
        }
        
        createImageBitmap(imageData).then(bitmap => {
            this.ctx.imageSmoothingEnabled = false;
            this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
            this.ctx.drawImage(bitmap, 0, 0, this.canvas.width, this.canvas.height);
        });
        
        this.updateFrameInfo();
    }

    depthToColor(normalized) {
        // Viridis colormap approximation
        const viridis = [
            [0.267004, 0.004874, 0.329415],
            [0.282623, 0.140926, 0.457517],
            [0.253935, 0.265254, 0.529983],
            [0.206756, 0.371758, 0.553117],
            [0.163625, 0.471133, 0.558148],
            [0.127568, 0.566949, 0.550556],
            [0.134692, 0.658636, 0.517649],
            [0.266941, 0.748751, 0.440573],
            [0.477504, 0.821444, 0.318195],
            [0.741388, 0.873449, 0.149561],
            [0.993248, 0.906157, 0.143936]
        ];
        
        const idx = normalized * (viridis.length - 1);
        const i = Math.floor(idx);
        const t = idx - i;
        
        if (i >= viridis.length - 1) {
            return {
                r: Math.round(viridis[viridis.length - 1][0] * 255),
                g: Math.round(viridis[viridis.length - 1][1] * 255),
                b: Math.round(viridis[viridis.length - 1][2] * 255)
            };
        }
        
        const c0 = viridis[i];
        const c1 = viridis[i + 1];
        
        return {
            r: Math.round(((c0[0] * (1 - t) + c1[0] * t)) * 255),
            g: Math.round(((c0[1] * (1 - t) + c1[1] * t)) * 255),
            b: Math.round(((c0[2] * (1 - t) + c1[2] * t)) * 255)
        };
    }

    updateFrameInfo() {
        document.getElementById('frameCounter').textContent = 
            `Frame: ${this.currentFrame + 1} / ${this.frames.length}`;
    }

    togglePlayPause() {
        if (this.isPlaying) {
            this.pause();
        } else {
            this.play();
        }
    }

    play() {
        if (this.frames.length === 0) return;
        
        this.isPlaying = true;
        document.getElementById('playPauseBtn').textContent = '⏸️ Pause';
        
        const animate = () => {
            if (!this.isPlaying) return;
            
            // Find next active frame
            let nextFrame = this.currentFrame + 1;
            while (nextFrame < this.frames.length && !this.frames[nextFrame].active) {
                nextFrame++;
            }
            
            if (nextFrame >= this.frames.length) {
                // Loop back to first active frame
                nextFrame = 0;
                while (nextFrame < this.frames.length && !this.frames[nextFrame].active) {
                    nextFrame++;
                }
            }
            
            if (nextFrame < this.frames.length) {
                this.currentFrame = nextFrame;
                this.renderFrame();
                this.drawTimeline();
            }
            
            setTimeout(() => requestAnimationFrame(animate), 1000 / this.fps);
        };
        
        animate();
    }

    pause() {
        this.isPlaying = false;
        document.getElementById('playPauseBtn').textContent = '▶️ Play';
    }

    prevFrame() {
        if (this.frames.length === 0) return;
        
        // Skip inactive frames
        let newFrame = this.currentFrame - 1;
        while (newFrame >= 0 && !this.frames[newFrame].active) {
            newFrame--;
        }
        if (newFrame >= 0) {
            this.currentFrame = newFrame;
        }
        
        this.ensureFrameVisible();
        this.renderFrame();
        this.drawTimeline();
    }

    nextFrame() {
        if (this.frames.length === 0) return;
        
        // Skip inactive frames
        let newFrame = this.currentFrame + 1;
        while (newFrame < this.frames.length && !this.frames[newFrame].active) {
            newFrame++;
        }
        if (newFrame < this.frames.length) {
            this.currentFrame = newFrame;
        }
        
        this.ensureFrameVisible();
        this.renderFrame();
        this.drawTimeline();
    }

    ensureFrameVisible() {
        if (this.timelineZoom <= 1) return;
        
        const totalFrames = this.frames.length;
        const visibleFrames = totalFrames / this.timelineZoom;
        const maxStartFrame = Math.max(0, totalFrames - visibleFrames);
        const startFrame = Math.floor(this.timelineScroll * maxStartFrame);
        const endFrame = startFrame + visibleFrames;
        
        // If current frame is outside visible range, scroll to it
        if (this.currentFrame < startFrame) {
            this.timelineScroll = this.currentFrame / maxStartFrame;
            this.timelineScrollbar.value = this.timelineScroll;
        } else if (this.currentFrame >= endFrame) {
            this.timelineScroll = (this.currentFrame - visibleFrames + 1) / maxStartFrame;
            this.timelineScrollbar.value = Math.min(1, this.timelineScroll);
        }
    }

    updateSpeed(event) {
        this.fps = parseInt(event.target.value);
        document.getElementById('speedValue').textContent = `${this.fps} FPS`;
    }

    updateZoom(event) {
        this.timelineZoom = parseFloat(event.target.value);
        document.getElementById('zoomValue').textContent = `${this.timelineZoom.toFixed(1)}x`;
        this.updateScrollbarRange();
        this.drawTimeline();
    }

    updateScrollbarRange() {
        const scrollbar = this.timelineScrollbar;
        scrollbar.max = 1;
        scrollbar.step = 0.01;
        scrollbar.disabled = this.timelineZoom <= 1;
        if (this.timelineScroll > 1) {
            this.timelineScroll = 1;
            scrollbar.value = 1;
        }
    }

    onTimelineWheel(event) {
        event.preventDefault();
        if (event.ctrlKey || event.metaKey) {
            // Zoom
            const delta = -event.deltaY * 0.01;
            this.timelineZoom = Math.max(1, Math.min(20, this.timelineZoom + delta));
            document.getElementById('zoomSlider').value = this.timelineZoom;
            document.getElementById('zoomValue').textContent = `${this.timelineZoom.toFixed(1)}x`;
            this.updateScrollbarRange();
            this.drawTimeline();
        } else {
            // Scroll
            this.timelineScroll = Math.max(0, Math.min(1, this.timelineScroll + event.deltaY * 0.001));
            this.timelineScrollbar.value = this.timelineScroll;
            this.drawTimeline();
        }
    }

    drawTimeline() {
        const w = this.timelineCanvas.width;
        const h = this.timelineCanvas.height;
        const ctx = this.timelineCtx;
        
        ctx.clearRect(0, 0, w, h);
        
        ctx.fillStyle = '#f0f0f0';
        ctx.fillRect(0, 0, w, h);
        
        if (this.frames.length === 0) return;
        
        // Calculate visible range based on zoom and scroll
        const totalFrames = this.frames.length;
        const visibleFrames = totalFrames / this.timelineZoom;
        const maxStartFrame = Math.max(0, totalFrames - visibleFrames);
        const startFrame = Math.floor(this.timelineScroll * maxStartFrame);
        const endFrame = Math.min(totalFrames, Math.ceil(startFrame + visibleFrames));
        
        const frameWidth = w / visibleFrames;
        
        // Draw frames with gaps for cut sections
        let blockStart = null;
        for (let i = startFrame; i < endFrame; i++) {
            const x = (i - startFrame) * frameWidth;
            
            if (this.frames[i].active) {
                if (blockStart === null) blockStart = i;
                ctx.fillStyle = i % 10 === 0 ? '#bbb' : '#ddd';
                ctx.fillRect(x, 0, Math.max(1, frameWidth - 1), h);
            } else {
                // Cut frame - show as dark gap
                if (blockStart !== null) {
                    // Draw block border
                    ctx.strokeStyle = '#667eea';
                    ctx.lineWidth = 3;
                    ctx.strokeRect((blockStart - startFrame) * frameWidth, 0, (i - blockStart) * frameWidth, h);
                    blockStart = null;
                }
                ctx.fillStyle = '#444';
                ctx.fillRect(x, 0, Math.max(1, frameWidth - 1), h);
            }
        }
        
        // Draw final block border
        if (blockStart !== null) {
            ctx.strokeStyle = '#667eea';
            ctx.lineWidth = 3;
            ctx.strokeRect((blockStart - startFrame) * frameWidth, 0, (endFrame - blockStart) * frameWidth, h);
        }
        
        // Draw selection
        if (this.selection.start !== null && this.selection.end !== null) {
            const start = Math.min(this.selection.start, this.selection.end);
            const end = Math.max(this.selection.start, this.selection.end);
            if (end >= startFrame && start < endFrame) {
                const selStart = Math.max(start, startFrame);
                const selEnd = Math.min(end + 1, endFrame);
                ctx.fillStyle = 'rgba(255, 200, 0, 0.4)';
                ctx.fillRect((selStart - startFrame) * frameWidth, 0, (selEnd - selStart) * frameWidth, h);
            }
        }
        
        // Draw playhead - always show if in visible range
        if (this.currentFrame >= startFrame && this.currentFrame < endFrame) {
            ctx.fillStyle = 'rgba(255, 0, 0, 0.8)';
            const playheadX = (this.currentFrame - startFrame) * frameWidth;
            ctx.fillRect(playheadX, 0, Math.max(2, Math.min(frameWidth, 3)), h);
        }
        
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 2;
        ctx.strokeRect(0, 0, w, h);
        
        // Update playhead indicator on scrollbar
        this.updatePlayheadIndicator();
    }

    updatePlayheadIndicator() {
        const indicator = document.getElementById('playheadIndicator');
        if (this.frames.length === 0) {
            indicator.style.display = 'none';
            return;
        }
        
        indicator.style.display = 'block';
        const position = (this.currentFrame / this.frames.length) * 100;
        indicator.style.left = `${position}%`;
    }

    onTimelineMouseDown(event) {
        const rect = this.timelineCanvas.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const frame = this.pixelToFrame(x);
        
        this.isDragging = true;
        this.selection.start = frame;
        this.selection.end = frame;
        this.drawTimeline();
    }

    onTimelineMouseMove(event) {
        if (!this.isDragging) return;
        
        const rect = this.timelineCanvas.getBoundingClientRect();
        const x = event.clientX - rect.left;
        const frame = this.pixelToFrame(x);
        
        this.selection.end = Math.max(0, Math.min(this.frames.length - 1, frame));
        this.drawTimeline();
        this.updateSelectionInfo();
    }

    onTimelineMouseUp(event) {
        this.isDragging = false;
        this.updateSelectionInfo();
        
        if (this.selection.start !== null && this.selection.end !== null) {
            document.getElementById('cutBtn').disabled = false;
            document.getElementById('deleteBtn').disabled = false;
            document.getElementById('clearSelectionBtn').disabled = false;
        }
    }

    onTimelineClick(event) {
        if (this.selection.start === this.selection.end) {
            const rect = this.timelineCanvas.getBoundingClientRect();
            const x = event.clientX - rect.left;
            this.currentFrame = this.pixelToFrame(x);
            this.ensureFrameVisible();
            this.renderFrame();
            this.drawTimeline();
        }
    }

    pixelToFrame(x) {
        const totalFrames = this.frames.length;
        const visibleFrames = totalFrames / this.timelineZoom;
        const maxStartFrame = Math.max(0, totalFrames - visibleFrames);
        const startFrame = Math.floor(this.timelineScroll * maxStartFrame);
        const frame = Math.floor((x / this.timelineCanvas.width) * visibleFrames + startFrame);
        return Math.max(0, Math.min(totalFrames - 1, frame));
    }

    updateSelectionInfo() {
        if (this.selection.start === null || this.selection.end === null) {
            document.getElementById('selectionInfo').textContent = 'No selection';
            return;
        }
        
        const start = Math.min(this.selection.start, this.selection.end);
        const end = Math.max(this.selection.start, this.selection.end);
        const count = end - start + 1;
        
        document.getElementById('selectionInfo').textContent = 
            `Selected: Frames ${start + 1} - ${end + 1} (${count} frames)`;
    }

    saveHistory() {
        this.history.push(JSON.parse(JSON.stringify(this.frames)));
        document.getElementById('undoBtn').disabled = false;
    }

    undo() {
        if (this.history.length === 0) return;
        
        this.frames = this.history.pop();
        if (this.history.length === 0) {
            document.getElementById('undoBtn').disabled = true;
        }
        
        this.currentFrame = Math.min(this.currentFrame, this.frames.length - 1);
        this.clearSelection();
        this.renderFrame();
        this.drawTimeline();
    }

    cutSelection() {
        if (this.selection.start === null || this.selection.end === null) return;
        
        this.saveHistory();
        
        const start = Math.min(this.selection.start, this.selection.end);
        const end = Math.max(this.selection.start, this.selection.end);
        
        for (let i = start; i <= end; i++) {
            this.frames[i].active = false;
        }
        
        this.clearSelection();
        this.renderFrame();
        this.drawTimeline();
    }

    deleteSelection() {
        if (this.selection.start === null || this.selection.end === null) return;
        
        this.saveHistory();
        
        const start = Math.min(this.selection.start, this.selection.end);
        const end = Math.max(this.selection.start, this.selection.end);
        
        this.frames.splice(start, end - start + 1);
        
        this.currentFrame = Math.min(this.currentFrame, this.frames.length - 1);
        this.clearSelection();
        this.renderFrame();
        this.drawTimeline();
    }

    clearSelection() {
        this.selection.start = null;
        this.selection.end = null;
        document.getElementById('cutBtn').disabled = true;
        document.getElementById('deleteBtn').disabled = true;
        document.getElementById('clearSelectionBtn').disabled = true;
        this.updateSelectionInfo();
        this.drawTimeline();
    }

    exportData() {
        if (this.frames.length === 0) return;
        
        let csv = '# Depth Sensor Log\n';
        csv += `# Binning Factor: ${this.metadata.binningFactor}\n`;
        csv += '# Frame,Steering,Throttle,Width,Height,Data...\n';
        
        const activeFrames = this.frames.filter(f => f.active);
        activeFrames.forEach((frame, index) => {
            const row = [
                index,
                frame.steering,
                frame.throttle,
                frame.width,
                frame.height,
                ...frame.data
            ].join(',');
            csv += row + '\n';
        });
        
        const blob = new Blob([csv], { type: 'text/csv' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `edited_log_${Date.now()}.csv`;
        a.click();
        URL.revokeObjectURL(url);
    }
}

const editor = new DatasetEditor();
