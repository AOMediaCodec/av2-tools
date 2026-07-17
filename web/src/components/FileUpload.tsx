import { useState, useCallback } from 'react';
import './FileUpload.css';

interface FileUploadProps {
  onFileLoaded: (data: Uint8Array, filename: string) => void;
  disabled?: boolean;
}

export function FileUpload({ onFileLoaded, disabled }: FileUploadProps) {
  const [dragActive, setDragActive] = useState(false);

  const handleFile = useCallback(
    (file: File) => {
      const reader = new FileReader();
      reader.onload = (e) => {
        const arrayBuffer = e.target?.result as ArrayBuffer;
        const uint8Array = new Uint8Array(arrayBuffer);
        onFileLoaded(uint8Array, file.name);
      };
      reader.onerror = () => {
        alert(`Failed to read file: ${reader.error?.message}`);
      };
      reader.readAsArrayBuffer(file);
    },
    [onFileLoaded]
  );

  const handleDrag = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  }, []);

  const handleDrop = useCallback(
    (e: React.DragEvent) => {
      e.preventDefault();
      e.stopPropagation();
      setDragActive(false);

      if (disabled) return;

      const files = e.dataTransfer?.files;
      if (files && files.length > 0) {
        handleFile(files[0]);
      }
    },
    [disabled, handleFile]
  );

  const handleChange = useCallback(
    (e: React.ChangeEvent<HTMLInputElement>) => {
      e.preventDefault();
      if (disabled) return;

      const files = e.target.files;
      if (files && files.length > 0) {
        handleFile(files[0]);
      }
    },
    [disabled, handleFile]
  );

  return (
    <div
      className={`file-upload ${dragActive ? 'drag-active' : ''} ${disabled ? 'disabled' : ''}`}
      onDragEnter={handleDrag}
      onDragLeave={handleDrag}
      onDragOver={handleDrag}
      onDrop={handleDrop}
    >
      <input
        type="file"
        id="file-input"
        accept=".bin,.av2,.obu,.mp4,.m4s,.m4v,.mov"
        onChange={handleChange}
        disabled={disabled}
      />
      <label htmlFor="file-input">
        <div className="upload-icon">📁</div>
        <div className="upload-text">
          <strong>Drop AV2 bitstream here</strong> or click to browse
          <div className="upload-hint">.obu elementary stream or .mp4 (AV2 track)</div>
        </div>
      </label>
    </div>
  );
}
