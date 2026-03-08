{
  description = "A Pointy Stick, A Garbage Collecting Allocator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    systems.url = "github:nix-systems/default";
  };

  outputs =
    {
      self,
      nixpkgs,
      systems,
    }:
    let
      inherit (nixpkgs) lib;
      forEachPkgs = f: lib.genAttrs (import systems) (system: f nixpkgs.legacyPackages.${system});
    in
    {
      devShells = forEachPkgs (pkgs: {
        default = pkgs.mkShell {
          buildInputs = with pkgs; [
            just
            gcc
          ];
        };
      });

    };
}
